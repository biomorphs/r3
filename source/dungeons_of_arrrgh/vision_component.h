#pragma once

#include "entities/component_helpers.h"
#include "core/glm_headers.h"
#include <unordered_set>

// tracks visible tiles + entities around the entity (uses transform component)
class DungeonsVisionComponent
{
public:
	static std::string_view GetTypeName() { return "DungeonsVisionComponent"; }
	static void RegisterScripts(R3::LuaSystem&);
	void SerialiseJson(R3::JsonSerialiser& s);
	void Inspect(const R3::Entities::EntityHandle& e, R3::Entities::World* w, R3::ValueInspector& i);

	bool m_needsUpdate = true;
	float m_visionMaxDistance = 8.0f;
	std::unordered_set<glm::uvec2> m_visibleTiles;	// updated when needsUpdate is true during fixed update, not serialised
};