#pragma once
#include "entities/component_helpers.h"

// used to identify monster actors in the world
class DungeonsMonsterComponent
{
public:
	static std::string_view GetTypeName() { return "Dungeons_Monster"; }
	static void RegisterScripts(R3::LuaSystem&);
	void SerialiseJson(R3::JsonSerialiser& s);
	void Inspect(const R3::Entities::EntityHandle& e, R3::Entities::World* w, R3::ValueInspector& i);
	std::string m_name;
};