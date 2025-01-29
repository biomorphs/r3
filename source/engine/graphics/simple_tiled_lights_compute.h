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

		static const uint32_t c_lightTileDimensions = 32;	// must always be a multiple of 16 and never < 16
		static const uint32_t c_maxTiledLights = ((3840 * 2160) / (c_lightTileDimensions * c_lightTileDimensions)) * 256;	// enough for 256 lights per tile at 4k
		struct LightTile					// one per tile
		{
			uint32_t m_firstLightIndex = 0;		// offset into m_lightIndexBuffer
			uint32_t m_lightIndexCount = 0;		// num indices
		};

		// Contains the info associated with a light tiling pass
		struct LightTileMetaData
		{
			VkDeviceAddress m_lightTileBuffer;	// a buffer of m_tileCount[0] * m_tileCount[1] instances of LightTile
			VkDeviceAddress m_lightIndexBuffer;	// a buffer of c_maxTiledLights uint16 indices into the lights themselves.
			uint32_t m_tileCount[2] = { 0,0 };
			uint32_t m_tileSize = c_lightTileDimensions;
		};

		void Cleanup(Device&);

		void DebugDrawLightTiles(glm::uvec2 screenDimensions, const Camera& camera, const std::vector<LightTile>& tiles, const std::vector<uint16_t>& indices);
		void BuildLightTilesCpu(glm::uvec2 screenDimensions, const Camera& camera, std::vector<LightTile>& tiles, std::vector<uint16_t>& indices);

		// used with BuildLightTilesCpu. Returns address to a LightTileMetaData buffer uploaded to gpu
		VkDeviceAddress CopyCpuDataToGpu(Device& d, VkCommandBuffer cmds, glm::uvec2 screenDimensions, const std::vector<LightTile>& tiles, const std::vector<uint16_t>& indices);

	private:
		std::unique_ptr<BufferPool> m_lightTileBufferPool;	// tile buffers are allocated/released here
	};
}