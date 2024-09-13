#pragma once

#include "engine/systems.h"
#include "engine/serialiser.h"
#include "entities/entity_handle.h"
#include "core/glm_headers.h"
#include <unordered_map>
#include <unordered_set>

namespace R3::Entities {
	class World;
};

// Game entry point
class DungeonsOfArrrgh : public R3::System
{
public:
	DungeonsOfArrrgh();
	virtual ~DungeonsOfArrrgh();
	static std::string_view GetName() { return "DungeonsOfArrrgh"; }
	virtual void RegisterTickFns();
	virtual bool Init();
	virtual void Shutdown();
	std::optional<glm::uvec2> GetTileFromWorldspace(class DungeonsWorldGridComponent& grid, glm::vec3 worldspace);
	std::optional<glm::uvec2> GetTileUnderMouseCursor(class DungeonsWorldGridComponent& grid);
private:	
	void SetEntityTilePosition(DungeonsWorldGridComponent& grid, R3::Entities::World& w, R3::Entities::EntityHandle e, uint32_t tileX, uint32_t tileZ);
	void SetVisualEntitiesVisible(const DungeonsWorldGridComponent& grid, R3::Entities::World& w, const std::unordered_set<glm::uvec2>& tiles, bool visible);
	void UpdateVision(class DungeonsWorldGridComponent& grid, R3::Entities::World& w);
	void DebugDrawVisibleTiles(const class DungeonsWorldGridComponent& grid, R3::Entities::World& w);
	void MoveEntitiesWorldspace(const std::vector<R3::Entities::EntityHandle>& targets, glm::vec3 offset);
	void GenerateTileVisuals(uint32_t x, uint32_t z, class DungeonsWorldGridComponent& grid, std::vector<R3::Entities::EntityHandle>& outEntities);
	void GenerateWorldVisuals(const R3::Entities::EntityHandle& e, class DungeonsWorldGridComponent& grid);
	void OnWorldGridDirty(const R3::Entities::EntityHandle& e, class DungeonsWorldGridComponent& grid);
	bool VariableUpdate();
	bool FixedUpdate();
	void DebugDrawWorldGrid(const class DungeonsWorldGridComponent& grid);
	template<class Container>
	void DebugDrawTiles(const class DungeonsWorldGridComponent& grid, Container& tiles);	// expects vector<uvec2> or similar
	std::unordered_map<std::string, nlohmann::json> m_generateVisualsEntityCache;	// cache serialised entities for speed
	glm::vec3 m_wsGridOffset = { 0,0,0 };
	glm::vec2 m_wsGridScale = { 4, 4 };
	bool m_debugDrawVisibleTiles = false;
};