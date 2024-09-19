#pragma once
#include "entities/component_helpers.h"
#include "engine/tag_set.h"
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
		union {	// tile flags stored in a single uint16_t (union for easy serialisation)
			uint32_t m_rawData;				// note that reordering flags will break serialised data
			struct TileData {
				bool m_passable : 1;		// can actors enter this tile (is it walkable)
				bool m_blockVisibility : 1;	// can actors see past this tile
			} m_flags;
		};
		using TileTags = R3::TagSet<4>;
		TileTags m_tags;					// used to generate visuals
		std::vector<R3::Entities::EntityHandle> m_entitiesInTile;
		R3::Entities::EntityHandle m_visualEntity;	// root of any visual entities
	};

	using VisibleTiles = std::unordered_set<glm::uvec2>;
	VisibleTiles FindVisibleTiles(glm::ivec2 startTile, uint32_t distance);		// results include the initial tile, if visible
	void ResizeGrid(glm::uvec2 size);
	glm::uvec2 GetDimensions() const { return m_gridDimensions; }
	const WorldTileContents* GetContents(uint32_t tileX, uint32_t tileZ) const;	// assume it will be sparse later
	WorldTileContents* GetContents(uint32_t tileX, uint32_t tileZ);
	R3::Entities::EntityHandle GetVisualEntity(uint32_t tileX, uint32_t tileZ);
	std::vector<R3::Entities::EntityHandle> GetEntitiesInTile(uint32_t tileX, uint32_t tileZ);
	std::string GetTileTagsAsString(uint32_t tileX, uint32_t tileZ);			// mainly for debugging
	bool TileHasTags(uint32_t tilex, uint32_t tiley);
	bool IsTilePassable(uint32_t tilex, uint32_t tiley);
	bool TileBlocksVision(uint32_t tilex, uint32_t tiley);
	void Fill(glm::uvec2 start, glm::uvec2 size, const WorldTileContents::TileTags& tags, bool isPassable, bool blockVisibility);
	bool AllTilesPassable(glm::uvec2 start, glm::uvec2 size);	// return true if all tiles in range are walkable
	// ignoreTargetBlockingFlags - calculate a path to the tile even if it is a blocker
	// by default, pathing to a blocking tile will always fail
	std::vector<glm::uvec2> CalculatePath(glm::uvec2 start, glm::uvec2 end, bool ignoreTargetBlockingFlags = false);	// find shortest path between tiles, taking in to account passable flag
	bool m_isDirty = false;				// if true, the world needs regeneration
	bool m_debugDraw = false;			// not serialised
private:
	glm::uvec2 m_gridDimensions;		// num tiles in x, z directions
	std::vector<WorldTileContents> m_worldGridContents;	// use GetContents for access
};