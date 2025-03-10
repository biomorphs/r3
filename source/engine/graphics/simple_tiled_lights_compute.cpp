#include "simple_tiled_lights_compute.h"
#include "engine/systems/lights_system.h"
#include "engine/systems/camera_system.h"
#include "render/linear_write_gpu_buffer.h"
#include "render/device.h"
#include "render/render_target_cache.h"
#include "render/descriptors.h"
#include "render/render_system.h"
#include "core/profiler.h"
#include "core/log.h"

namespace R3
{
	struct BuildFrustumPushConstants
	{
		glm::mat4 m_inverseProjViewMatrix;		
		glm::vec4 m_eyeWorldSpacePosition;		// w is unused
		glm::vec2 m_screenDimensions;
		uint32_t m_tileCount[2];
		VkDeviceAddress m_tileFrustumBuffer;	// preallocated 
	};

	struct BuildLightTilePushConstants
	{
		VkDeviceAddress m_allLightsBuffer;
		VkDeviceAddress m_tileFrustumsBuffer;
		VkDeviceAddress m_lightIndexBuffer;
		VkDeviceAddress m_lightTileBuffer;
		uint32_t m_tileCount[2];
	};

	struct LightTileDebugPushConstants
	{
		VkDeviceAddress m_lightTileMetadata;
	};

	void TiledLightsCompute::Cleanup(Device& d)
	{
		vkDestroyPipelineLayout(d.GetVkDevice(), m_pipelineLayoutFrustumBuild, nullptr);
		vkDestroyPipeline(d.GetVkDevice(), m_pipelineFrustumBuild, nullptr);
		vkDestroyPipelineLayout(d.GetVkDevice(), m_pipelineLayoutTileData, nullptr);
		vkDestroyPipeline(d.GetVkDevice(), m_pipelineTileData, nullptr);
		vkDestroyPipelineLayout(d.GetVkDevice(), m_pipelineLayoutDebug, nullptr);
		vkDestroyPipeline(d.GetVkDevice(), m_pipelineDebug, nullptr);
		vkDestroyDescriptorSetLayout(d.GetVkDevice(), m_debugDescriptorLayout, nullptr);
		vkDestroyDescriptorSetLayout(d.GetVkDevice(), m_frustumDescriptorLayout, nullptr);
		vkDestroySampler(d.GetVkDevice(), m_depthSampler, nullptr);
		m_descriptorAllocator = {};
	}

	bool TiledLightsCompute::Initialise(Device& d)
	{
		R3_PROF_EVENT();

		m_descriptorAllocator = std::make_unique<DescriptorSetSimpleAllocator>();
		std::vector<VkDescriptorPoolSize> poolSizes = {
			{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, c_maxSets },
			{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, c_maxSets }					// input + output images
		};
		if (!m_descriptorAllocator->Initialise(d, c_maxSets * 2, poolSizes))
		{
			LogError("Failed to create descriptor allocator");
			return false;
		}
		{
			DescriptorLayoutBuilder layoutBuilder;
			layoutBuilder.AddBinding(0, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);		// output image
			m_debugDescriptorLayout = layoutBuilder.Create(d, false);				// dont need bindless here
			if (m_debugDescriptorLayout == nullptr)
			{
				LogError("Failed to create descriptor set layout");
				return false;
			}
			for (uint32_t i = 0; i < c_maxSets; ++i)
			{
				m_debugDescriptorSets[i] = m_descriptorAllocator->Allocate(d, m_debugDescriptorLayout);
			}
		}
		{
			DescriptorLayoutBuilder layoutBuilder;
			layoutBuilder.AddBinding(0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);		// depth buffer
			m_frustumDescriptorLayout = layoutBuilder.Create(d, false);				// dont need bindless here
			if (m_frustumDescriptorLayout == nullptr)
			{
				LogError("Failed to create descriptor set layout");
				return false;
			}
			for (uint32_t i = 0; i < c_maxSets; ++i)
			{
				m_frustumDescriptorSets[i] = m_descriptorAllocator->Allocate(d, m_frustumDescriptorLayout);
			}
		}
		{
			// create depth sampler
			VkSamplerCreateInfo sampler = {};
			sampler.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
			sampler.magFilter = VK_FILTER_NEAREST;
			sampler.minFilter = VK_FILTER_NEAREST;
			sampler.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
			sampler.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
			sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
			sampler.maxLod = VK_LOD_CLAMP_NONE;
			sampler.minLod = 0;
			if (!VulkanHelpers::CheckResult(vkCreateSampler(d.GetVkDevice(), &sampler, nullptr, &m_depthSampler)))
			{
				LogError("Failed to create sampler");
				return false;
			}
		}
		{
			auto frustumComputeShader = VulkanHelpers::LoadShaderModule(d.GetVkDevice(), "shaders_spirv/common/build_light_tile_frustums.comp.spv");
			if (frustumComputeShader == VK_NULL_HANDLE)
			{
				LogError("Failed to load light tile frustum shader");
				return false;
			}
			VkPushConstantRange constantRange;
			constantRange.offset = 0;
			constantRange.size = sizeof(BuildFrustumPushConstants);
			constantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
			VkPipelineLayoutCreateInfo pipelineLayoutInfoFrustums = { 0 };
			pipelineLayoutInfoFrustums.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
			pipelineLayoutInfoFrustums.pushConstantRangeCount = 1;
			pipelineLayoutInfoFrustums.pPushConstantRanges = &constantRange;
			pipelineLayoutInfoFrustums.setLayoutCount = 1;
			pipelineLayoutInfoFrustums.pSetLayouts = &m_frustumDescriptorLayout;
			if (!VulkanHelpers::CheckResult(vkCreatePipelineLayout(d.GetVkDevice(), &pipelineLayoutInfoFrustums, nullptr, &m_pipelineLayoutFrustumBuild)))
			{
				LogError("Failed to create pipeline layout");
				return false;
			}
			m_pipelineFrustumBuild = VulkanHelpers::CreateComputePipeline(d.GetVkDevice(), frustumComputeShader, m_pipelineLayoutFrustumBuild, "main");
			vkDestroyShaderModule(d.GetVkDevice(), frustumComputeShader, nullptr);
		}
		{
			auto tileDataShader = VulkanHelpers::LoadShaderModule(d.GetVkDevice(), "shaders_spirv/common/build_light_tiles_from_frustums.comp.spv");
			if (tileDataShader == VK_NULL_HANDLE)
			{
				LogError("Failed to load light tile shader");
				return false;
			}
			VkPushConstantRange constantRange;
			constantRange.offset = 0;
			constantRange.size = sizeof(BuildLightTilePushConstants);
			constantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
			VkPipelineLayoutCreateInfo pipelineLayoutInfo = { 0 };
			pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
			pipelineLayoutInfo.pushConstantRangeCount = 1;
			pipelineLayoutInfo.pPushConstantRanges = &constantRange;
			if (!VulkanHelpers::CheckResult(vkCreatePipelineLayout(d.GetVkDevice(), &pipelineLayoutInfo, nullptr, &m_pipelineLayoutTileData)))
			{
				LogError("Failed to create pipeline layout");
				return false;
			}
			m_pipelineTileData = VulkanHelpers::CreateComputePipeline(d.GetVkDevice(), tileDataShader, m_pipelineLayoutTileData, "main");
			vkDestroyShaderModule(d.GetVkDevice(), tileDataShader, nullptr);
		}
		{
			auto debugShader = VulkanHelpers::LoadShaderModule(d.GetVkDevice(), "shaders_spirv/common/light_tile_debug_output.comp.spv");
			if (debugShader == VK_NULL_HANDLE)
			{
				LogError("Failed to load light tile debug shader");
				return false;
			}
			VkPushConstantRange constantRange;
			constantRange.offset = 0;
			constantRange.size = sizeof(LightTileDebugPushConstants);
			constantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
			VkPipelineLayoutCreateInfo pipelineLayoutInfo = { 0 };
			pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
			pipelineLayoutInfo.setLayoutCount = 1;
			pipelineLayoutInfo.pSetLayouts= &m_debugDescriptorLayout;
			pipelineLayoutInfo.pushConstantRangeCount = 1;
			pipelineLayoutInfo.pPushConstantRanges = &constantRange;
			if (!VulkanHelpers::CheckResult(vkCreatePipelineLayout(d.GetVkDevice(), &pipelineLayoutInfo, nullptr, &m_pipelineLayoutDebug)))
			{
				LogError("Failed to create pipeline layout");
				return false;
			}
			m_pipelineDebug = VulkanHelpers::CreateComputePipeline(d.GetVkDevice(), debugShader, m_pipelineLayoutDebug, "main");
			vkDestroyShaderModule(d.GetVkDevice(), debugShader, nullptr);
		}

		m_initialised = true;
		return true;
	}

	VkDeviceAddress TiledLightsCompute::BuildTileFrustumsCompute(Device& d, VkCommandBuffer cmds, RenderTarget& depthBuffer, glm::uvec2 screenDimensions, const Camera& camera)
	{
		R3_PROF_EVENT();
		VkDeviceAddress address = 0;
		const uint32_t c_tilesX = (uint32_t)glm::ceil(screenDimensions.x / (float)c_lightTileDimensions);
		const uint32_t c_tilesY = (uint32_t)glm::ceil(screenDimensions.y / (float)c_lightTileDimensions);

		// We need a frustum buffer big enough for all the tiles
		auto bufferPool = Systems::GetSystem<RenderSystem>()->GetBufferPool();
		auto frustumBuffer = bufferPool->GetBuffer("Tiled Light Frustums", c_tilesX * c_tilesY * sizeof(LightTileFrustum), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_AUTO, false);
		if (!frustumBuffer)
		{
			LogError("Failed to allocate tile frustum buffer");
			return address;
		}

		// If there are no lights, just allocate the buffer
		if (Systems::GetSystem<LightsSystem>()->GetActivePointLights() > 0)
		{
			VulkanHelpers::CommandBufferRegionLabel cmdLabel(cmds, "BuildTileFrustums", { 1,1,0,1 });

			const glm::mat4 projViewMat = camera.ProjectionMatrix() * camera.ViewMatrix();
			const glm::mat4 inverseProjView = glm::inverse(projViewMat);

			// Write a descriptor set each frame
			DescriptorSetWriter writer(m_frustumDescriptorSets[m_currentSet]);
			writer.WriteImage(0, 0, depthBuffer.m_view, m_depthSampler, depthBuffer.m_lastLayout);
			writer.FlushWrites();

			// build a frustum for each tile
			BuildFrustumPushConstants pc;
			pc.m_tileFrustumBuffer = frustumBuffer->m_deviceAddress;
			pc.m_eyeWorldSpacePosition = glm::vec4(camera.Position(), 0.0f);
			pc.m_screenDimensions = glm::vec2(screenDimensions);
			pc.m_inverseProjViewMatrix = inverseProjView;
			pc.m_tileCount[0] = c_tilesX;
			pc.m_tileCount[1] = c_tilesY;
			vkCmdBindPipeline(cmds, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipelineFrustumBuild);
			vkCmdPushConstants(cmds, m_pipelineLayoutFrustumBuild, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pc), &pc);
			vkCmdBindDescriptorSets(cmds, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipelineLayoutFrustumBuild, 0, 1, &m_frustumDescriptorSets[m_currentSet], 0, nullptr);
			vkCmdDispatch(cmds, c_tilesX, c_tilesY, 1);	// one invocation per pixel

			// memory barrier between compute stages
			VulkanHelpers::DoMemoryBarrier(cmds, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT);
		}

		address = frustumBuffer->m_deviceAddress;
		bufferPool->Release(*frustumBuffer);	// release the buffer back to the pool for a future frame

		return address;
	}

	void TiledLightsCompute::BuildTileDataCompute(Device& d, VkCommandBuffer cmds, glm::uvec2 screenDimensions, VkDeviceAddress tileFrustums, VkDeviceAddress lightTileBuffer, VkDeviceAddress lightIndexBuffer)
	{
		R3_PROF_EVENT();

		const uint32_t c_tilesX = (uint32_t)glm::ceil(screenDimensions.x / (float)c_lightTileDimensions);
		const uint32_t c_tilesY = (uint32_t)glm::ceil(screenDimensions.y / (float)c_lightTileDimensions);

		VulkanHelpers::CommandBufferRegionLabel cmdLabel(cmds, "BuildLightTileData", { 1,0.7,0,1 });

		// build a list of light indices for each tile
		BuildLightTilePushConstants pc;
		pc.m_tileCount[0] = c_tilesX;
		pc.m_tileCount[1] = c_tilesY;
		pc.m_tileFrustumsBuffer = tileFrustums;
		pc.m_lightTileBuffer = lightTileBuffer;
		pc.m_lightIndexBuffer = lightIndexBuffer;
		pc.m_allLightsBuffer = Systems::GetSystem<LightsSystem>()->GetAllLightsDeviceAddress();

		vkCmdBindPipeline(cmds, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipelineTileData);
		vkCmdPushConstants(cmds, m_pipelineLayoutTileData, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pc), &pc);
		vkCmdDispatch(cmds, c_tilesX, c_tilesY, 1);

		// memory barrier between compute stages
		VulkanHelpers::DoMemoryBarrier(cmds, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT);
	}

	// tile list building runs in 2 compute passes
	// the first pass builds a world-space frustum for each screen tile
	// the 2nd pass tests each light against each frustum and appends indices to the light-list
	VkDeviceAddress TiledLightsCompute::BuildTilesListCompute(Device& d, VkCommandBuffer cmds, RenderTarget& depthBuffer, glm::uvec2 screenDimensions, const Camera& camera)
	{
		R3_PROF_EVENT();
		if (!m_initialised)
		{
			if (!Initialise(d))
			{
				return 0;
			}
		}

		const uint32_t c_tilesX = (uint32_t)glm::ceil(screenDimensions.x / (float)c_lightTileDimensions);
		const uint32_t c_tilesY = (uint32_t)glm::ceil(screenDimensions.y / (float)c_lightTileDimensions);

		// first build frustum for each tile
		VkDeviceAddress frustumBuffer = BuildTileFrustumsCompute(d, cmds, depthBuffer, screenDimensions, camera);
		if (frustumBuffer == 0)
		{
			LogError("Failed to build tile frustums");
			return 0;
		}

		// allocate light data buffers
		auto bufferPool = Systems::GetSystem<RenderSystem>()->GetBufferPool();
		auto lightTileBuffer = bufferPool->GetBuffer("Light Tiles", c_tilesX * c_tilesY * sizeof(LightTile), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_AUTO, false);
		if (!lightTileBuffer)
		{
			LogError("Failed to allocate light tile buffer");
			return 0;
		}
		WriteOnlyGpuBuffer lightIndexBuffer;	// use a write-only buffer with tiny staging buffer so we can write 0 to the count first
		if(!lightIndexBuffer.Create("Light Index Buffer", d, (1 + c_maxTiledLights) * sizeof(uint32_t), sizeof(uint32_t), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT))
		{
			LogError("Failed to allocate light index buffer");
			return 0;
		}
		uint32_t resetCount = 0;
		lightIndexBuffer.Allocate(sizeof(resetCount));
		lightIndexBuffer.Write(0, sizeof(resetCount), &resetCount);
		lightIndexBuffer.Flush(d, cmds, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

		// build light tile data in compute...
		if (Systems::GetSystem<LightsSystem>()->GetActivePointLights() > 0)
		{
			BuildTileDataCompute(d, cmds, screenDimensions, frustumBuffer, lightTileBuffer->m_deviceAddress, lightIndexBuffer.GetDataDeviceAddress());
		}
		else
		{
			// just clear the tile buffer with 0s if there are no point lights
			vkCmdFillBuffer(cmds, lightTileBuffer->m_buffer.m_buffer, 0, c_tilesX * c_tilesY * sizeof(LightTile), 0);

			// FillBuffer is a transfer operation, make sure we do the proper barrier
			VulkanHelpers::DoMemoryBarrier(cmds, VK_PIPELINE_STAGE_2_TRANSFER_BIT, 
				VK_ACCESS_2_MEMORY_WRITE_BIT, 
				VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, 
				VK_ACCESS_2_MEMORY_READ_BIT);
		}

		// build metadata object to pass to lighting shaders
		LightTileMetaData metadata;
		VkDeviceAddress metadataAddress = 0;
		metadata.m_tileCount[0] = (uint32_t)glm::ceil(screenDimensions.x / (float)c_lightTileDimensions);
		metadata.m_tileCount[1] = (uint32_t)glm::ceil(screenDimensions.y / (float)c_lightTileDimensions);
		metadata.m_lightIndexBuffer = lightIndexBuffer.GetDataDeviceAddress();
		metadata.m_lightTileBuffer = lightTileBuffer->m_deviceAddress;
		LinearWriteGpuBuffer metadataBuffer;
		if (metadataBuffer.Create("Light Tile Metadata", d, sizeof(LightTileMetaData), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT))
		{
			metadataAddress = metadataBuffer.GetBufferDeviceAddress();
			metadataBuffer.Write(sizeof(LightTileMetaData), &metadata);
			metadataBuffer.Flush(d, cmds, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
		}
		else
		{
			LogError("Failed to allocate light tile metadata buffer");
		}

		// release all temp buffers back to the pool
		bufferPool->Release(*lightTileBuffer);
		lightIndexBuffer.Destroy(d);
		metadataBuffer.Destroy(d);

		if (++m_currentSet >= c_maxSets)
		{
			m_currentSet = 0;
		}

		return metadataAddress;
	}

	void TiledLightsCompute::ShowTilesDebug(Device& d, VkCommandBuffer cmds, RenderTarget& outputTarget, glm::vec2 outputDimensions, VkDeviceAddress lightTileMetadata)
	{
		R3_PROF_EVENT();
		if (!m_initialised)
		{
			if (!Initialise(d))
			{
				return;
			}
		}
		// Write a descriptor set each frame
		DescriptorSetWriter writer(m_debugDescriptorSets[m_currentSet]);
		writer.WriteStorageImage(0, outputTarget.m_view, outputTarget.m_lastLayout);
		writer.FlushWrites();

		LightTileDebugPushConstants pc;
		pc.m_lightTileMetadata = lightTileMetadata;

		vkCmdBindPipeline(cmds, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipelineDebug);
		vkCmdBindDescriptorSets(cmds, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipelineLayoutDebug, 0, 1, &m_debugDescriptorSets[m_currentSet], 0, nullptr);
		vkCmdPushConstants(cmds, m_pipelineLayoutDebug, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pc), &pc);
		vkCmdDispatch(cmds, (uint32_t)glm::ceil(outputDimensions.x / 16.0f), (uint32_t)glm::ceil(outputDimensions.y / 16.0f), 1);
	}
}