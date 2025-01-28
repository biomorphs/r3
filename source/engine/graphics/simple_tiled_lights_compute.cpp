#include "simple_tiled_lights_compute.h"
#include "engine/systems/lights_system.h"
#include "engine/systems/camera_system.h"
#include "engine/systems/immediate_render_system.h"
#include "engine/utils/frustum.h"
#include "render/linear_write_gpu_buffer.h"
#include "core/profiler.h"
#include "core/log.h"

namespace R3
{
	void TiledLightsCompute::Cleanup(Device&)
	{
		m_lightTileBufferPool = {};
	}

	VkDeviceAddress TiledLightsCompute::CopyCpuDataToGpu(Device& d, VkCommandBuffer cmds, glm::uvec2 screenDimensions, const std::vector<LightTile>& tileData)
	{
		R3_PROF_EVENT();
		VkDeviceAddress address = 0;
		if (m_lightTileBufferPool == nullptr)
		{
			m_lightTileBufferPool = std::make_unique<BufferPool>("Light Tile Buffers");
		}

		const uint32_t c_tilesX = (uint32_t)glm::ceil(screenDimensions.x / (float)c_lightTileDimensions);
		const uint32_t c_tilesY = (uint32_t)glm::ceil(screenDimensions.y / (float)c_lightTileDimensions);
		assert(tileData.size() == (c_tilesX * c_tilesY));

		// upload the light tile data + metadata
		LinearWriteOnlyGpuArray<LightTile> lightTileBuffer;
		LightTileMetaData metadata;
		if (lightTileBuffer.Create(d, c_tilesX * c_tilesY, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, m_lightTileBufferPool.get()))
		{
			lightTileBuffer.Write((uint32_t)tileData.size(), tileData.data());
			lightTileBuffer.Flush(d, cmds, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
			metadata.m_lightTileBuffer = lightTileBuffer.GetBufferDeviceAddress();
			lightTileBuffer.Destroy(d);	// this does not actually destroy anything, but it releases the buffers back to the pools
		}
		else
		{
			LogError("Failed to allocate light tile buffer");
		}

		metadata.m_tileCount[0] = c_tilesX;
		metadata.m_tileCount[1] = c_tilesY;
		metadata.m_screenResolution[0] = screenDimensions.x;
		metadata.m_screenResolution[1] = screenDimensions.y;
		
		LinearWriteGpuBuffer metadataBuffer;
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

	void TiledLightsCompute::DebugDrawLightTiles(glm::uvec2 screenDimensions, const Camera& camera, const std::vector<LightTile>& tiles)
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
				if (thisTileLightData.m_lightCount > 0)
				{
					glm::vec2 negativeStep = (2.0f * glm::vec2(x, y)) / glm::vec2(c_tilesX, c_tilesY);
					glm::vec2 positiveStep = (2.0f * glm::vec2(x + 1, y + 1)) / glm::vec2(c_tilesX, c_tilesY);

					// build clip-space points just off near plane
					glm::vec3 farPoints[4] =
					{
						{ -1.0f + negativeStep.x, -1.0f + negativeStep.y, 0.01f },		// top left 
						{ -1.0f + positiveStep.x, -1.0f + negativeStep.y, 0.01f },		// top right
						{ -1.0f + negativeStep.x, -1.0f + positiveStep.y, 0.01f },		// botton left 
						{ -1.0f + positiveStep.x, -1.0f + positiveStep.y, 0.01f },		// botton right
					};

					glm::vec4 c_pallete[] = {
						{0.0f,0.0f,0.1f,c_maxLightsPerTile / 32},
						{0.0f,1.0f,0.1f,c_maxLightsPerTile / 8},
						{1.0f,1.0f,0.0f,c_maxLightsPerTile / 4},
						{1.0f,0.0f,0.0f,c_maxLightsPerTile / 2},
						{1.0f,1.0f,1.0f,c_maxLightsPerTile}
					};
					glm::vec3 colour = { 0,0,0.1f };
					for (int i = 0; i < (int)std::size(c_pallete) - 1; ++i)
					{
						if ((thisTileLightData.m_lightCount > c_pallete[i].w) && (thisTileLightData.m_lightCount <= c_pallete[i + 1].w))
						{
							const float mixVal = (float)(thisTileLightData.m_lightCount - c_pallete[i].w) / (c_pallete[i + 1].w - c_pallete[i].w);
							colour = glm::mix(glm::vec3(c_pallete[i]), glm::vec3(c_pallete[i + 1]), mixVal);
						}
					}

					// convert the points to world-space
					for (int i = 0; i < 4; ++i)
					{
						glm::vec4 projected = inverseProjView * glm::vec4(farPoints[i], 1.0f);
						farPoints[i] = glm::vec3(projected / projected.w);	// perspective divide

						quadVerts[i].m_position = glm::vec4(farPoints[i], 1.0f);
						quadVerts[i].m_colour = glm::vec4(colour,0.25f);
					}

					imRender->AddTriangle(quadVerts[0], quadVerts[1], quadVerts[2]);
					imRender->AddTriangle(quadVerts[1], quadVerts[3], quadVerts[2]);
				}
			}
		}
	}

	std::vector<TiledLightsCompute::LightTile> TiledLightsCompute::BuildLightTilesCpu(glm::uvec2 screenDimensions, const Camera& camera)
	{
		R3_PROF_EVENT();
		std::vector<LightTile> allTiles;
		const std::vector<Pointlight>& activePointlights = Systems::GetSystem<LightsSystem>()->GetActivePointLights();

		// how many tiles do we need
		const uint32_t c_tilesX = (uint32_t)glm::ceil(screenDimensions.x / (float)c_lightTileDimensions);
		const uint32_t c_tilesY = (uint32_t)glm::ceil(screenDimensions.y / (float)c_lightTileDimensions);
		allTiles.resize(c_tilesX * c_tilesY);

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
					// build a frustum in clip space then convert it to world space using inverse of proj-view matrix

					// Split frustum into 'steps'/cells on x/y axis
					glm::vec2 negativeStep = (2.0f * glm::vec2(tileX, tileY)) / glm::vec2(c_tilesX, c_tilesY);
					glm::vec2 positiveStep = (2.0f * glm::vec2(tileX + 1, tileY + 1)) / glm::vec2(c_tilesX, c_tilesY);

					// build clip-space points on far plane which we use to build the frustum planes
					glm::vec3 farPoints[4] =
					{
						{ -1.0f + negativeStep.x, -1.0f + negativeStep.y, 1.0f },		// top left 
						{ -1.0f + positiveStep.x, -1.0f + negativeStep.y, 1.0f },		// top right
						{ -1.0f + negativeStep.x, -1.0f + positiveStep.y, 1.0f },		// botton left 
						{ -1.0f + positiveStep.x, -1.0f + positiveStep.y, 1.0f },		// botton right
					};

					// convert the points to world-space
					for (int i = 0; i < 4; ++i)
					{
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
					LightTile& thisTileLightData = allTiles[tileX + (tileY * c_tilesX)];
					for (uint32_t l = 0; l < activePointlights.size() && thisTileLightData.m_lightCount < c_maxLightsPerTile; ++l)
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
							thisTileLightData.m_lightIndices[thisTileLightData.m_lightCount++] = l;
						}
					}
				}
			}
		}

		return allTiles;
	}
}