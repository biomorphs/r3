#pragma once
#include "entities/component_helpers.h"
#include <unordered_set>

class DungeonsWorldGridComponent
{
public:
	static std::string_view GetTypeName() { return "Dungeons_WorldGridComponent"; }
	static void RegisterScripts(R3::LuaSystem&);
	void SerialiseJson(R3::JsonSerialiser& s);
	void Inspect(const R3::Entities::EntityHandle& e, R3::Entities::World* w, R3::ValueInspector& i);

	struct WorldTileContents {	// 1 per tile
		void SerialiseJson(R3::JsonSerialiser& s);
		union {	// tile data stored in a single uint64_t (union for easy serialisation)
			uint64_t m_rawData;				// note that reordering flags will break serialised data
			struct TileData {
				uint8_t m_tileType;			// 1 byte for 255 different types of tile
				bool m_passable : 1;		// can actors enter this tile (is it walkable)
				bool m_blockVisibility : 1;	// can actors see past this tile
			} m_tileData;
		};
		std::vector<R3::Entities::EntityHandle> m_entitiesInTile;
		R3::Entities::EntityHandle m_visualEntity;	// root of any visual entities
	};

	using VisibleTiles = std::unordered_set<glm::uvec2>;
	VisibleTiles FindVisibleTiles(glm::ivec2 startTile, uint32_t distance);		// results include the initial tile, if visible
	void ResizeGrid(glm::uvec2 size);
	glm::uvec2 GetDimensions() const { return m_gridDimensions; }
	const WorldTileContents* GetContents(uint32_t tileX, uint32_t tileZ) const;	// assume it will be sparse later
	WorldTileContents* GetContents(uint32_t tileX, uint32_t tileZ);
	uint8_t GetTileType(uint32_t tilex, uint32_t tiley);
	bool IsTilePassable(uint32_t tilex, uint32_t tiley);
	void Fill(glm::uvec2 start, glm::uvec2 size, uint8_t type, bool isPassable, bool blockVisibility);
	bool AllTilesMatchType(glm::uvec2 start, glm::uvec2 size, uint8_t type);	// return true if all tiles in range match the type
	std::vector<glm::uvec2> CalculatePath(glm::uvec2 start, glm::uvec2 end);	// find shortest path between tiles, taking in to account passable flag
	bool m_isDirty = false;				// if true, the world needs regeneration
	bool m_debugDraw = false;			// not serialised
private:
	glm::uvec2 m_gridDimensions;		// num tiles in x, z directions
	std::vector<WorldTileContents> m_worldGridContents;	// use GetContents for access
};