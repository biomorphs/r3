#pragma once 
#include "core/glm_headers.h"
#include "render/buffer_pool.h"

namespace R3
{
	const uint16_t c_maxLightsPerTile = 128;
	const uint32_t c_lightTileDimensions = 32;	// c_lightTileDimensions x c_lightTileDimensions tiles

	struct LightTileData
	{
		uint16_t m_lightCount = 0;		// num lights in this tile
		uint16_t m_lightIndices[c_maxLightsPerTile] = { 0 };
	};

	class Device;
	class Camera;

	// A compute or CPU pass that culls lights against a screen-space grid of tiles
	// Lights are stored in-line in the light buffer
	// Can be used multiple times/frame for different render targets/cameras
	// No depth bounds checks! Super simple light grid
	class SimpleTiledLightsCompute
	{
	public:
		SimpleTiledLightsCompute() = default;
		~SimpleTiledLightsCompute() = default;

		void Reset();	// call at start or end of frame to release previously built buffers
		void Cleanup(Device&);

		void DebugDrawLightTiles(glm::uvec2 screenDimensions, const Camera& camera, const std::vector<LightTileData>& tiles);
		std::vector<LightTileData> BuildMainCameraLightTilesCpu(glm::uvec2 screenDimensions, const Camera& camera);
		std::vector<LightTileData> BuildLightTileDataCpu(glm::uvec2 screenDimensions, const Camera& camera);	// returns an array of tiled lights, computed on CPU

	private:
	};
}