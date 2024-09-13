#pragma once

#include "core/glm_headers.h"
#include "entities/component_helpers.h"

// any entity 'on' a world grid must have one of these
// the actual tile position according to the world can be very different to the regular transform used for rendering
// an entity can only be in one tile!
class DungeonsWorldGridPosition
{
friend class DungeonsOfArrrgh;	// gross
public:
	static std::string_view GetTypeName() { return "Dungeons_WorldGridPosition"; }
	static void RegisterScripts(R3::LuaSystem&);
	void SerialiseJson(R3::JsonSerialiser& s);
	void Inspect(const R3::Entities::EntityHandle& e, R3::Entities::World* w, R3::ValueInspector& i);
	glm::uvec2 GetPosition() { return m_position; }	// read-only
private:
	glm::uvec2 m_position = { -1,-1 };	// no direct access, go through the world grid API
};