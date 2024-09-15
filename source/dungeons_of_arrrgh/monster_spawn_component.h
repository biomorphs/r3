#pragma once
#include "entities/component_helpers.h"

// monsters are a bit more special and want their own persistent spawner object
class DungeonsMonsterSpawner
{
public:
	static std::string_view GetTypeName() { return "Dungeons_MonsterSpawner"; }
	static void RegisterScripts(R3::LuaSystem&);
	void SerialiseJson(R3::JsonSerialiser& s);
	void Inspect(const R3::Entities::EntityHandle& e, R3::Entities::World* w, R3::ValueInspector& i);
	std::string m_monsterType;		// game can reinterpret this however it wants
	glm::uvec2 m_spawnPosition;		// position on the grid, this entity does not need a transform
};