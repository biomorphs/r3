#pragma once 
#include "core/glm_headers.h"
#include "render/buffer_pool.h"
#include <memory>

namespace R3
{
	class Device;
	class Camera;

	// A compute or CPU pass that culls lights against a screen-space grid of tiles
	// Lights are stored in-line in the light buffer
	// Can be used multiple times/frame for different render targets/cameras
	// No depth bounds checks! Super simple light grid
	class TiledLightsCompute
	{
	public:
		TiledLightsCompute() = default;
		~TiledLightsCompute() = default;

		static const uint16_t c_maxLightsPerTile = 127;
		static const uint32_t c_lightTileDimensions = 32;	// must always be a multiple of 16 and never < 16
		struct LightTile					// one per tile
		{
			uint16_t m_lightCount = 0;		// num lights in this tile
			uint16_t m_lightIndices[c_maxLightsPerTile] = { 0 };
		};

		// Contains the info associated with a light tiling pass
		struct LightTileMetaData
		{
			VkDeviceAddress m_lightTileBuffer;	// a buffer of m_tileCount[0] * m_tileCount[1] instances of LightTile
			uint32_t m_screenResolution[2] = { 0,0 };
			uint32_t m_tileCount[2] = { 0,0 };
			uint32_t m_tileSize = c_lightTileDimensions;
		};

		void Cleanup(Device&);

		void DebugDrawLightTiles(glm::uvec2 screenDimensions, const Camera& camera, const std::vector<LightTile>& tiles);
		std::vector<LightTile> BuildLightTilesCpu(glm::uvec2 screenDimensions, const Camera& camera);	// returns an array of tiled lights, computed on CPU

		// used with BuildLightTilesCpu. Returns address to a LightTileMetaData buffer uploaded to gpu
		VkDeviceAddress CopyCpuDataToGpu(Device& d, VkCommandBuffer cmds, glm::uvec2 screenDimensions, const std::vector<LightTile>& tileData);

	private:
		std::unique_ptr<BufferPool> m_lightTileBufferPool;	// tile buffers are allocated/released here
	};
}