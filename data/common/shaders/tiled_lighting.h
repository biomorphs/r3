// Size of light tiles in pixels
#define COMPUTE_LIGHT_TILE_SIZE 16

// light indices stored in one giant buffer of uints
layout(buffer_reference, std430) readonly buffer LightTileIndexBuffer { 
	uint m_totalCount;			// used when writing indices from compute
	uint data[];
};

// one per tile
struct LightTile				
{
	uint m_firstLightIndex;		// offset into LightTileIndexBuffer
	uint m_lightIndexCount;		// num indices (i.e. num lights)
};

// all tiles stored in one buffer
layout(buffer_reference, std430) readonly buffer LightTileBuffer { 
	LightTile data[];
};

// used in tile building
struct LightTileFrustum
{
	vec4 m_planes[5];			// world-space frustum planes, top, bottom, left, right, far plane
};

// describes all light tile data
struct LightTileMetadata		
{
	LightTileBuffer m_lightTiles;	// a buffer of m_tileCount[0] * m_tileCount[1] instances of LightTile
	LightTileIndexBuffer m_lightIndices;
	uint m_tileCount[2];
};

// Metadata for light tiles
layout(buffer_reference, std430) readonly buffer LightTileMetadataBuffer { 
	LightTileMetadata data[];
};

// Get index into tile data for a position on screen
uint GetLightTileIndex(uvec2 pos, uint tileCount[2])
{
	uvec2 tileIndex2D = pos / COMPUTE_LIGHT_TILE_SIZE;
	return tileIndex2D.x + (tileIndex2D.y * tileCount[0]);
}