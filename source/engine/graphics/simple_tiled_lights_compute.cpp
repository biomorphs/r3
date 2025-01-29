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

	VkDeviceAddress TiledLightsCompute::CopyCpuDataToGpu(Device& d, VkCommandBuffer cmds, glm::uvec2 screenDimensions, const std::vector<LightTile>& tiles, const std::vector<uint16_t>& indices)
	{
		R3_PROF_EVENT();
		VkDeviceAddress address = 0;
		if (m_lightTileBufferPool == nullptr)
		{
			m_lightTileBufferPool = std::make_unique<BufferPool>("Light Tile Buffers");
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

		LinearWriteOnlyGpuArray<uint16_t> lightIndexBuffer;
		if (lightIndexBuffer.Create(d, c_maxTiledLights, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, m_lightTileBufferPool.get()))
		{
			if (indices.size() > 0)
			{
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

	void TiledLightsCompute::DebugDrawLightTiles(glm::uvec2 screenDimensions, const Camera& camera, const std::vector<LightTile>& tiles, const std::vector<uint16_t>& indices)
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

					// convert the points to world-space
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

	void TiledLightsCompute::BuildLightTilesCpu(glm::uvec2 screenDimensions, const Camera& camera, std::vector<LightTile>& tiles, std::vector<uint16_t>& indices)
	{
		R3_PROF_EVENT();
		const std::vector<Pointlight>& activePointlights = Systems::GetSystem<LightsSystem>()->GetActivePointLights();

		// how many tiles do we need
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
					// build a frustum in clip space then convert it to world space

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