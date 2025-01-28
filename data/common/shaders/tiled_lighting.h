#extension GL_EXT_shader_16bit_storage : require

struct LightTile				// one per tile
{
	uint16_t m_lightCount;			// num lights in this tile
	uint16_t m_lightIndices[127];	// index into global lights list
};

// all tiles stored in one buffer
layout(buffer_reference, std430) readonly buffer LightTileBuffer { 
	LightTile data[];
};

struct LightTileMetadata		// describes all light tile data
{
	LightTileBuffer m_lightTiles;	// a buffer of m_tileCount[0] * m_tileCount[1] instances of LightTile
	uint m_screenResolution[2];
	uint m_tileCount[2];
	uint m_tileDimensions;
};

// Metadata for light tiles
layout(buffer_reference, std430) readonly buffer LightTileMetadataBuffer { 
	LightTileMetadata data[];
};

// Get index into tile data for a position on screen
uint GetLightTileIndex(uvec2 pos, LightTileMetadata metadata)
{
	uvec2 tileIndex2D = pos / metadata.m_tileDimensions;
	return tileIndex2D.x + (tileIndex2D.y * metadata.m_tileCount[0]);
}