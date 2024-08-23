#pragma once

#include "engine/systems.h"
#include "engine/serialiser.h"
#include "entities/entity_handle.h"
#include "core/glm_headers.h"
#include <unordered_map>
#include <vector>

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
private:
	void GenerateWorld(class DungeonsWorldGridComponent& grid);
	void MoveEntities(const std::vector<R3::Entities::EntityHandle>& targets, glm::vec3 offset);
	void GenerateTileVisuals(uint32_t x, uint32_t z, class DungeonsWorldGridComponent& grid, std::vector<R3::Entities::EntityHandle>& outEntities);
	void GenerateWorldVisuals(const R3::Entities::EntityHandle& e, class DungeonsWorldGridComponent& grid);
	void OnWorldGridDirty(const R3::Entities::EntityHandle& e, class DungeonsWorldGridComponent& grid);
	bool VariableUpdate();
	void DebugDrawWorldGrid(const class DungeonsWorldGridComponent& grid);
	std::unordered_map<std::string, nlohmann::json> m_generateVisualsEntityCache;	// cache serialised entities for speed
	glm::vec3 m_drawGridOffset = { 0,0,0 };
	glm::vec2 m_drawGridScale = { 4, 4 };
	float m_drawBlockerHeight = 3.0f;
};