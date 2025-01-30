#pragma once 
#include "core/glm_headers.h"
#include "render/buffer_pool.h"
#include <memory>

namespace R3
{
	class Device;
	class Camera;

	// A compute or CPU pass that culls lights against a screen-space grid of tiles
	// Lights are stored as a buffer of indices, with index offset+size stored per tile
	// Can be used multiple times/frame for different render targets/cameras
	class TiledLightsCompute
	{
	public:
		TiledLightsCompute() = default;
		~TiledLightsCompute() = default;

		static const uint32_t c_lightTileDimensions = 16;	// must always be a multiple of 16 and never < 16. must match COMPUTE_LIGHT_TILE_SIZE in shaders!
		static const uint32_t c_maxTiledLights = ((3840 * 2160) / (c_lightTileDimensions * c_lightTileDimensions)) * 256;	// enough for 256 lights per tile at 4k
		struct LightTile						// one per tile
		{
			uint32_t m_firstLightIndex = 0;		// offset into m_lightIndexBuffer
			uint32_t m_lightIndexCount = 0;		// num indices
		};

		// Contains the info associated with a light tiling pass
		struct LightTileMetaData
		{
			VkDeviceAddress m_lightTileBuffer;	// a buffer of m_tileCount[0] * m_tileCount[1] instances of LightTile
			VkDeviceAddress m_lightIndexBuffer;	// a buffer of 1 + c_maxTiledLights uint32 indices into the lights themselves (first entry is total count).
			uint32_t m_tileCount[2] = { 0,0 };
		};

		void Cleanup(Device&);

		// Cpu-based light tile builder
		void DebugDrawLightTiles(glm::uvec2 screenDimensions, const Camera& camera, const std::vector<LightTile>& tiles, const std::vector<uint32_t>& indices);
		void BuildLightTilesCpu(glm::uvec2 screenDimensions, const Camera& camera, std::vector<LightTile>& tiles, std::vector<uint32_t>& indices);

		// returns an address to a LightTileMetaData buffer
		VkDeviceAddress CopyCpuDataToGpu(Device& d, VkCommandBuffer cmds, glm::uvec2 screenDimensions, const std::vector<LightTile>& tiles, const std::vector<uint32_t>& indices);

		// run the entire process on gpu, returns an address to a LightTileMetaData object
		VkDeviceAddress BuildTilesListCompute(Device& d, VkCommandBuffer cmds, glm::uvec2 screenDimensions, const Camera& camera);

	private:
		// returns a buffer containing a LightTileFrustum per tile
		struct LightTileFrustum
		{
			glm::vec4 m_planes[4];
		};
		VkDeviceAddress BuildTileFrustumsCompute(Device& d, VkCommandBuffer cmds, glm::uvec2 screenDimensions, const Camera& camera);

		// fills in lightTileBuffer and lightIndexBuffer based on frustums in tileFrustums
		void BuildTileDataCompute(Device& d, VkCommandBuffer cmds, glm::uvec2 screenDimensions, VkDeviceAddress tileFrustums, VkDeviceAddress lightTileBuffer, VkDeviceAddress lightIndexBuffer);

		bool Initialise(Device& d);
		bool m_initialised = false;
		std::unique_ptr<BufferPool> m_lightTileBufferPool;	// tile buffers are allocated/released here

		// pipeline for the frustum builder
		VkPipelineLayout m_pipelineLayoutFrustumBuild = VK_NULL_HANDLE;
		VkPipeline m_pipelineFrustumBuild = VK_NULL_HANDLE;

		// pipeline for the tile data builder
		VkPipelineLayout m_pipelineLayoutTileData = VK_NULL_HANDLE;
		VkPipeline m_pipelineTileData = VK_NULL_HANDLE;
	};
}