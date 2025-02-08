#include "deferred_lighting_compute.h"
#include "engine/systems/lights_system.h"
#include "engine/systems/camera_system.h"
#include "render/device.h"
#include "render/render_target_cache.h"
#include "render/vulkan_helpers.h"
#include "core/profiler.h"
#include "core/log.h"

namespace R3
{
	// Pass lighting data buffer address via push constants
	struct PushConstants
	{
		glm::mat4 m_worldToViewTransform;		// used to calculate view-space depth for choosing cascades
		glm::vec4 m_cameraWorldSpacePosition;	// used to calculate view direction
		VkDeviceAddress m_lightingData;			// address of light data from lights system
		VkDeviceAddress m_tileMetadata;			// built by TiledLightsCompute
	};

	void DeferredLightingCompute::Run(Device& d, VkCommandBuffer cmds, 
		RenderTarget& depthBuffer,
		RenderTarget& positionMetalTarget,
		RenderTarget& normalRoughnessTarget,
		RenderTarget& albedoAOTarget,
		RenderTarget& outputTarget, glm::vec2 outputDimensions, bool useTiledLighting)
	{
		R3_PROF_EVENT();

		if (!m_resourcesInitialised)
		{
			if (!Initialise(d))
			{
				LogError("Failed to initialise tonemap compute shader!");
				return;
			}
		}

		// Write a descriptor set each frame containing the gbuffer + output textures
		DescriptorSetWriter writer(m_descriptorSets[m_currentSet]);
		writer.WriteStorageImage(0, positionMetalTarget.m_view, positionMetalTarget.m_lastLayout);
		writer.WriteStorageImage(1, normalRoughnessTarget.m_view, normalRoughnessTarget.m_lastLayout);
		writer.WriteStorageImage(2, albedoAOTarget.m_view, albedoAOTarget.m_lastLayout);
		writer.WriteImage(3, 0, depthBuffer.m_view, m_depthSampler, depthBuffer.m_lastLayout);
		writer.WriteStorageImage(4, outputTarget.m_view, outputTarget.m_lastLayout);
		writer.FlushWrites();

		auto mainCamera = Systems::GetSystem<CameraSystem>()->GetMainCamera();
		PushConstants pc;
		pc.m_worldToViewTransform = mainCamera.ViewMatrix();
		pc.m_cameraWorldSpacePosition = glm::vec4(mainCamera.Position(), 1);
		pc.m_lightingData = Systems::GetSystem<LightsSystem>()->GetAllLightsDeviceAddress();
		pc.m_tileMetadata = useTiledLighting ? m_lightTileMetadata : 0;

		if (useTiledLighting && m_lightTileMetadata != 0)
		{
			vkCmdBindPipeline(cmds, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipelineTiled);
		}
		else
		{
			vkCmdBindPipeline(cmds, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipelineAllLights);
		}
		vkCmdBindDescriptorSets(cmds, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipelineLayout, 0, 1, &m_descriptorSets[m_currentSet], 0, nullptr);
		auto shadowMapsDescriptorSet = Systems::GetSystem<LightsSystem>()->GetAllShadowMapsSet();
		vkCmdBindDescriptorSets(cmds, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipelineLayout, 1, 1, &shadowMapsDescriptorSet, 0, nullptr);
		vkCmdPushConstants(cmds, m_pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pc), &pc);
		vkCmdDispatch(cmds, (uint32_t)glm::ceil(outputDimensions.x / 16.0f), (uint32_t)glm::ceil(outputDimensions.y / 16.0f), 1);
		if (++m_currentSet >= c_maxSets)
		{
			m_currentSet = 0;
		}
	}

	bool DeferredLightingCompute::Initialise(Device& d)
	{
		R3_PROF_EVENT();
		m_descriptorAllocator = std::make_unique<DescriptorSetSimpleAllocator>();
		std::vector<VkDescriptorPoolSize> poolSizes = {
			{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 4 * c_maxSets },					// input + output images
			{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, c_maxSets }			// depth buffer
		};
		if (!m_descriptorAllocator->Initialise(d, c_maxSets, poolSizes))
		{
			LogError("Failed to create descriptor allocator");
			return false;
		}

		DescriptorLayoutBuilder layoutBuilder;
		layoutBuilder.AddBinding(0, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);		// pos + metal
		layoutBuilder.AddBinding(1, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);		// normal + roughness
		layoutBuilder.AddBinding(2, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);		// albedo + AO
		layoutBuilder.AddBinding(3, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);		// depth buffer texture
		layoutBuilder.AddBinding(4, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);		// output image
		m_descriptorLayout = layoutBuilder.Create(d, false);					// dont need bindless here
		if (m_descriptorLayout == nullptr)
		{
			LogError("Failed to create descriptor set layout");
			return false;
		}
		// Create the sets but don't write them yet
		for (uint32_t i = 0; i < c_maxSets; ++i)
		{
			m_descriptorSets[i] = m_descriptorAllocator->Allocate(d, m_descriptorLayout);
		}

		// create depth sampler
		{
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

		auto computeShaderTiled = VulkanHelpers::LoadShaderModule(d.GetVkDevice(), "shaders_spirv/common/deferred_lighting_compute_tiled.comp.spv");
		auto computeShaderAllLights = VulkanHelpers::LoadShaderModule(d.GetVkDevice(), "shaders_spirv/common/deferred_lighting_compute_all_lights.comp.spv");
		if (computeShaderAllLights == VK_NULL_HANDLE || computeShaderTiled == VK_NULL_HANDLE)
		{
			LogError("Failed to load tonemap shader");
			return false;
		}

		// Create the pipelines and layouts
		VkPushConstantRange constantRange;
		constantRange.offset = 0;
		constantRange.size = sizeof(PushConstants);
		constantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

		VkDescriptorSetLayout setLayouts[] = { m_descriptorLayout, Systems::GetSystem<LightsSystem>()->GetShadowMapDescriptorLayout() };

		VkPipelineLayoutCreateInfo pipelineLayoutInfo = { 0 };
		pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutInfo.pSetLayouts = setLayouts;
		pipelineLayoutInfo.setLayoutCount = std::size(setLayouts);
		pipelineLayoutInfo.pushConstantRangeCount = 1;
		pipelineLayoutInfo.pPushConstantRanges = &constantRange;
		if (!VulkanHelpers::CheckResult(vkCreatePipelineLayout(d.GetVkDevice(), &pipelineLayoutInfo, nullptr, &m_pipelineLayout)))
		{
			LogError("Failed to create pipeline layout");
			return false;
		}

		m_pipelineAllLights = VulkanHelpers::CreateComputePipeline(d.GetVkDevice(), computeShaderAllLights, m_pipelineLayout, "main");
		m_pipelineTiled = VulkanHelpers::CreateComputePipeline(d.GetVkDevice(), computeShaderTiled, m_pipelineLayout, "main");

		// We don't need the shader any more
		vkDestroyShaderModule(d.GetVkDevice(), computeShaderTiled, nullptr);
		vkDestroyShaderModule(d.GetVkDevice(), computeShaderAllLights, nullptr);
		m_resourcesInitialised = true;

		return true;
	}

	void DeferredLightingCompute::Cleanup(Device& d)
	{
		R3_PROF_EVENT();

		vkDestroySampler(d.GetVkDevice(), m_depthSampler, nullptr);

		// cleanup the pipelines
		vkDestroyPipeline(d.GetVkDevice(), m_pipelineAllLights, nullptr);
		vkDestroyPipeline(d.GetVkDevice(), m_pipelineTiled, nullptr);
		vkDestroyPipelineLayout(d.GetVkDevice(), m_pipelineLayout, nullptr);

		// cleanup the descriptors
		vkDestroyDescriptorSetLayout(d.GetVkDevice(), m_descriptorLayout, nullptr);
		m_descriptorAllocator = {};
	}
}