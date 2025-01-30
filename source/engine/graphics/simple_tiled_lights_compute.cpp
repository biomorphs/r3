#include "simple_tiled_lights_compute.h"
#include "engine/systems/lights_system.h"
#include "engine/systems/camera_system.h"
#include "engine/systems/immediate_render_system.h"
#include "engine/utils/frustum.h"
#include "render/linear_write_gpu_buffer.h"
#include "render/device.h"
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

	void TiledLightsCompute::Cleanup(Device& d)
	{
		vkDestroyPipelineLayout(d.GetVkDevice(), m_pipelineLayoutFrustumBuild, nullptr);
		vkDestroyPipeline(d.GetVkDevice(), m_pipelineFrustumBuild, nullptr);
		vkDestroyPipelineLayout(d.GetVkDevice(), m_pipelineLayoutTileData, nullptr);
		vkDestroyPipeline(d.GetVkDevice(), m_pipelineTileData, nullptr);
		m_lightTileBufferPool = {};
	}

	bool TiledLightsCompute::Initialise(Device& d)
	{
		R3_PROF_EVENT();

		if (m_lightTileBufferPool == nullptr)
		{
			m_lightTileBufferPool = std::make_unique<BufferPool>("Light Tile Buffers");
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

		m_initialised = true;
		return true;
	}

	VkDeviceAddress TiledLightsCompute::BuildTileFrustumsCompute(Device& d, VkCommandBuffer cmds, glm::uvec2 screenDimensions, const Camera& camera)
	{
		R3_PROF_EVENT();
		VkDeviceAddress address = 0;
		const uint32_t c_tilesX = (uint32_t)glm::ceil(screenDimensions.x / (float)c_lightTileDimensions);
		const uint32_t c_tilesY = (uint32_t)glm::ceil(screenDimensions.y / (float)c_lightTileDimensions);

		// We need a frustum buffer big enough for all the tiles
		auto frustumBuffer = m_lightTileBufferPool->GetBuffer(c_tilesX * c_tilesY * sizeof(LightTileFrustum), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_AUTO, false);
		if (!frustumBuffer)
		{
			LogError("Failed to allocate tile frustum buffer");
			return address;
		}

		// If there are no lights, just allocate the buffer
		if (Systems::GetSystem<LightsSystem>()->GetActivePointLights().size() > 0)
		{
			VulkanHelpers::CommandBufferRegionLabel cmdLabel(cmds, "BuildTileFrustums", { 1,1,0,1 });

			const glm::mat4 projViewMat = camera.ProjectionMatrix() * camera.ViewMatrix();
			const glm::mat4 inverseProjView = glm::inverse(projViewMat);

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
			vkCmdDispatch(cmds, (uint32_t)glm::ceil(c_tilesX / 16.0f), (uint32_t)glm::ceil(c_tilesY / 16.0f), 1);	// one invocation per tile

			// memory barrier between compute stages
			VkMemoryBarrier writeBarrier = { 0 };
			writeBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
			writeBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
			writeBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			vkCmdPipelineBarrier(cmds,
				VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,		// src stage
				VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,		// dst stage
				0,
				1,
				&writeBarrier,
				0, nullptr, 0, nullptr
			);
		}

		address = frustumBuffer->m_deviceAddress;
		m_lightTileBufferPool->Release(*frustumBuffer);	// release the buffer back to the pool for a future frame

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
		VkMemoryBarrier writeBarrier = { 0 };
		writeBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
		writeBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
		writeBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		vkCmdPipelineBarrier(cmds,
			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,		// src stage
			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,		// dst stage
			0,
			1,
			&writeBarrier,
			0, nullptr, 0, nullptr
		);
	}

	// tile list building runs in 2 compute passes
	// the first pass builds a world-space frustum for each screen tile
	// the 2nd pass tests each light against each frustum and appends indices to the light-list
	VkDeviceAddress TiledLightsCompute::BuildTilesListCompute(Device& d, VkCommandBuffer cmds, glm::uvec2 screenDimensions, const Camera& camera)
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
		VkDeviceAddress frustumBuffer = BuildTileFrustumsCompute(d, cmds, screenDimensions, camera);
		if (frustumBuffer == 0)
		{
			LogError("Failed to build tile frustums");
			return 0;
		}

		// allocate light data buffers
		auto lightTileBuffer = m_lightTileBufferPool->GetBuffer(c_tilesX * c_tilesY * sizeof(LightTile), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_AUTO, false);
		if (!lightTileBuffer)
		{
			LogError("Failed to allocate light tile buffer");
			return 0;
		}
		WriteOnlyGpuBuffer lightIndexBuffer;	// use a write-only buffer with tiny staging buffer so we can write 0 to the count first
		if(!lightIndexBuffer.Create(d, (1 + c_maxTiledLights) * sizeof(uint32_t), sizeof(uint32_t), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, m_lightTileBufferPool.get()))
		{
			LogError("Failed to allocate light index buffer");
			return 0;
		}
		uint32_t resetCount = 0;
		lightIndexBuffer.Allocate(sizeof(resetCount));
		lightIndexBuffer.Write(0, sizeof(resetCount), &resetCount);
		lightIndexBuffer.Flush(d, cmds, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

		// build light tile data in compute...
		if (Systems::GetSystem<LightsSystem>()->GetActivePointLights().size() > 0)
		{
			BuildTileDataCompute(d, cmds, screenDimensions, frustumBuffer, lightTileBuffer->m_deviceAddress, lightIndexBuffer.GetDataDeviceAddress());
		}

		// build metadata object to pass to lighting shaders
		LightTileMetaData metadata;
		VkDeviceAddress metadataAddress = 0;
		metadata.m_tileCount[0] = (uint32_t)glm::ceil(screenDimensions.x / (float)c_lightTileDimensions);
		metadata.m_tileCount[1] = (uint32_t)glm::ceil(screenDimensions.y / (float)c_lightTileDimensions);
		metadata.m_lightIndexBuffer = lightIndexBuffer.GetDataDeviceAddress();
		metadata.m_lightTileBuffer = lightTileBuffer->m_deviceAddress;
		LinearWriteGpuBuffer metadataBuffer;
		if (metadataBuffer.Create(d, sizeof(LightTileMetaData), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, m_lightTileBufferPool.get()))
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
		m_lightTileBufferPool->Release(*lightTileBuffer);
		lightIndexBuffer.Destroy(d);
		metadataBuffer.Destroy(d);

		return metadataAddress;
	}

	VkDeviceAddress TiledLightsCompute::CopyCpuDataToGpu(Device& d, VkCommandBuffer cmds, glm::uvec2 screenDimensions, const std::vector<LightTile>& tiles, const std::vector<uint32_t>& indices)
	{
		R3_PROF_EVENT();
		if (!m_initialised)
		{
			if (!Initialise(d))
			{
				return 0;
			}
		}

		LightTileMetaData metadata;
		const uint32_t c_tilesX = (uint32_t)glm::ceil(screenDimensions.x / (float)c_lightTileDimensions);
		const uint32_t c_tilesY = (uint32_t)glm::ceil(screenDimensions.y / (float)c_lightTileDimensions);
		assert(tiles.size() == (c_tilesX * c_tilesY));
		metadata.m_tileCount[0] = c_tilesX;
		metadata.m_tileCount[1] = c_tilesY;

		// upload the light tile data, indices, and metadata
		LinearWriteOnlyGpuArray<LightTile> lightTileBuffer;
		if (lightTileBuffer.Create(d, c_tilesX * c_tilesY, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, m_lightTileBufferPool.get()))
		{
			lightTileBuffer.Write((uint32_t)tiles.size(), tiles.data());
			lightTileBuffer.Flush(d, cmds, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
			metadata.m_lightTileBuffer = lightTileBuffer.GetBufferDeviceAddress();
			lightTileBuffer.Destroy(d);	// this does not actually destroy anything, but it releases the buffers back to the pools
		}
		else
		{
			LogError("Failed to allocate light tile buffer");
		}

		LinearWriteOnlyGpuArray<uint32_t> lightIndexBuffer;		// first index reserved for total index counter, only used in compute
		if (lightIndexBuffer.Create(d, 1 + c_maxTiledLights, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, m_lightTileBufferPool.get()))
		{
			if (indices.size() > 0)
			{
				uint32_t dummyCount = 0;
				lightIndexBuffer.Write(1, &dummyCount);
				lightIndexBuffer.Write((uint32_t)indices.size(), indices.data());
				lightIndexBuffer.Flush(d, cmds, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
			}
			metadata.m_lightIndexBuffer = lightIndexBuffer.GetBufferDeviceAddress();
			lightIndexBuffer.Destroy(d);	// this does not actually destroy anything, but it releases the buffers back to the pools
		}
		else
		{
			LogError("Failed to allocate light index buffer");
		}

		LinearWriteGpuBuffer metadataBuffer;
		VkDeviceAddress address = 0;
		if (metadataBuffer.Create(d, sizeof(LightTileMetaData), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, m_lightTileBufferPool.get()))
		{
			metadataBuffer.Write(sizeof(LightTileMetaData), &metadata);
			metadataBuffer.Flush(d, cmds, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
			address = metadataBuffer.GetBufferDeviceAddress();
			metadataBuffer.Destroy(d);	// release the buffer back to the pool
		}
		else
		{
			LogError("Failed to allocate light tile metadata buffer");
		}

		return address;
	}

	void TiledLightsCompute::DebugDrawLightTiles(glm::uvec2 screenDimensions, const Camera& camera, const std::vector<LightTile>& tiles, const std::vector<uint32_t>& indices)
	{
		// how many tiles do we need
		const uint32_t c_tilesX = (uint32_t)glm::ceil(screenDimensions.x / (float)c_lightTileDimensions);
		const uint32_t c_tilesY = (uint32_t)glm::ceil(screenDimensions.y / (float)c_lightTileDimensions);
		const glm::mat4 projViewMat = camera.ProjectionMatrix() * camera.ViewMatrix();
		const glm::mat4 inverseProjView = glm::inverse(projViewMat);
		auto& imRender = Systems::GetSystem<ImmediateRenderSystem>()->m_imRender;
		ImmediateRenderer::PosColourVertex quadVerts[4];
		for (uint32_t y = 0; y < c_tilesY; ++y)
		{
			for (uint32_t x = 0; x < c_tilesX; ++x)
			{
				const LightTile& thisTileLightData = tiles[x + (y * c_tilesX)];
				if (thisTileLightData.m_lightIndexCount > 0)
				{
					// build screen-space points just off near clip plane
					glm::vec3 farPoints[4] =
					{
						{ x * c_lightTileDimensions, y * c_lightTileDimensions, 0.01f },					// top left 
						{ (x + 1) * c_lightTileDimensions, y * c_lightTileDimensions, 0.01f },				// top right
						{ x * c_lightTileDimensions, (y + 1) * c_lightTileDimensions, 0.01f },				// botton left 
						{ (x + 1) * c_lightTileDimensions, (y + 1) * c_lightTileDimensions, 0.01f },		// botton right
					};

					// make a nice colour pallete
					glm::vec4 c_pallete[] = {
						{0.0f,0.0f,0.1f,0},
						{0.0f,1.0f,0.1f,64},
						{1.0f,1.0f,0.0f,128},
						{1.0f,0.0f,0.0f,256},
						{1.0f,1.0f,1.0f,1024}
					};
					glm::vec3 colour = { 0,0,0.1f };
					for (int i = 0; i < (int)std::size(c_pallete) - 1; ++i)
					{
						if ((thisTileLightData.m_lightIndexCount > c_pallete[i].w) && (thisTileLightData.m_lightIndexCount <= c_pallete[i + 1].w))
						{
							const float mixVal = (float)(thisTileLightData.m_lightIndexCount - c_pallete[i].w) / (c_pallete[i + 1].w - c_pallete[i].w);
							colour = glm::mix(glm::vec3(c_pallete[i]), glm::vec3(c_pallete[i + 1]), mixVal);
						}
					}

					// convert screen x,y to clip space, z is already on far clip plane!
					auto screenToClip = [&screenDimensions](glm::vec3 p)
					{
						return glm::vec3(((p.x / (float)screenDimensions.x) * 2.0f) - 1.0f, ((p.y / (float)screenDimensions.y) * 2.0f) - 1.0f, p.z);
					};
					for (int i = 0; i < 4; ++i)
					{
						farPoints[i] = screenToClip(farPoints[i]);
						glm::vec4 projected = inverseProjView * glm::vec4(farPoints[i], 1.0f);
						farPoints[i] = glm::vec3(projected / projected.w);	// perspective divide

						quadVerts[i].m_position = glm::vec4(farPoints[i], 1.0f);
						quadVerts[i].m_colour = glm::vec4(colour,0.5f);
					}
					imRender->AddTriangle(quadVerts[0], quadVerts[1], quadVerts[2]);
					imRender->AddTriangle(quadVerts[1], quadVerts[3], quadVerts[2]);
				}
			}
		}
	}

	void TiledLightsCompute::BuildLightTilesCpu(glm::uvec2 screenDimensions, const Camera& camera, std::vector<LightTile>& tiles, std::vector<uint32_t>& indices)
	{
		R3_PROF_EVENT();
		const std::vector<Pointlight>& activePointlights = Systems::GetSystem<LightsSystem>()->GetActivePointLights();
		const uint32_t c_tilesX = (uint32_t)glm::ceil(screenDimensions.x / (float)c_lightTileDimensions);
		const uint32_t c_tilesY = (uint32_t)glm::ceil(screenDimensions.y / (float)c_lightTileDimensions);
		tiles.resize(c_tilesX * c_tilesY);
		indices.clear();
		indices.reserve(c_maxTiledLights);
		if (activePointlights.size() > 0)
		{
			// we need inverse of view-projection matrix to go from clip-space to world-space
			const glm::mat4 projViewMat = camera.ProjectionMatrix() * camera.ViewMatrix();
			const glm::mat4 inverseProjView = glm::inverse(projViewMat);
			const glm::vec3 eyePosition = camera.Position();
			for (uint32_t tileY = 0; tileY < c_tilesY; ++tileY)
			{
				for (uint32_t tileX = 0; tileX < c_tilesX; ++tileX)
				{
					// start in screen space to ensure tiles are actually c_lightTileDimensions x c_lightTileDimensions
					glm::vec3 farPoints[4] =
					{
						{ tileX * c_lightTileDimensions, tileY * c_lightTileDimensions, 1.0f },					// top left 
						{ (tileX + 1) * c_lightTileDimensions, tileY * c_lightTileDimensions, 1.0f },			// top right
						{ tileX * c_lightTileDimensions, (tileY + 1) * c_lightTileDimensions, 1.0f },			// botton left 
						{ (tileX + 1) * c_lightTileDimensions, (tileY + 1) * c_lightTileDimensions, 1.0f },		// botton right
					};

					// convert screen x,y to clip space, z is already on far clip plane!
					auto screenToClip = [&screenDimensions](glm::vec3 p)
					{
						return glm::vec3(((p.x / (float)screenDimensions.x) * 2.0f) - 1.0f, ((p.y / (float)screenDimensions.y) * 2.0f) - 1.0f, p.z);
					};

					// convert the points clip-space, then to world-space
					for (int i = 0; i < 4; ++i)
					{
						farPoints[i] = screenToClip(farPoints[i]);
						glm::vec4 projected = inverseProjView * glm::vec4(farPoints[i], 1.0f);
						farPoints[i] = glm::vec3(projected / projected.w);	// perspective divide
					}

					// build frustum planes from the world-space points
					auto PointsToPlane = [](glm::vec3 p0, glm::vec3 p1, glm::vec3 p2) -> glm::vec4
					{
						glm::vec3 v0 = p1 - p0;
						glm::vec3 v1 = p2 - p0;
						glm::vec4 plane = glm::vec4(glm::normalize(glm::cross(v0, v1)), 0.0f);
						plane.w = glm::dot(glm::vec3(plane), p0);
						return plane;
					};
					glm::vec4 frustumPlanes[4] =
					{
						PointsToPlane(eyePosition, farPoints[2], farPoints[0]),	// left
						PointsToPlane(eyePosition, farPoints[1], farPoints[3]),	// right
						PointsToPlane(eyePosition, farPoints[0], farPoints[1]),	// top
						PointsToPlane(eyePosition, farPoints[3], farPoints[2])	// bottom
					};

					// now determine which lights intersect the frustum for this tile
					LightTile& thisTileLightData = tiles[tileX + (tileY * c_tilesX)];
					thisTileLightData.m_lightIndexCount = 0;
					thisTileLightData.m_firstLightIndex = (uint32_t)indices.size();
					for (uint32_t l = 0; l < activePointlights.size() && (indices.size() < c_maxTiledLights-1); ++l)
					{
						const glm::vec3 center = glm::vec3(activePointlights[l].m_positionDistance);
						const float radius = activePointlights[l].m_positionDistance.w;
						bool inFrustum = true;
						for (int i = 0; (i < 4) && inFrustum; ++i)
						{
							float d = glm::dot(glm::vec3(frustumPlanes[i]), center) - frustumPlanes[i].w;
							inFrustum = (d >= -radius);
						}
						if (inFrustum)
						{
							indices.push_back(l);
							thisTileLightData.m_lightIndexCount++;
						}
					}
				}
			}
		}
	}
}