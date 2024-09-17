#pragma once

#include "entities/component_helpers.h"

// if an actor can be inspected it will have one of these
class DungeonsInspectableComponent
{
public:
	static std::string_view GetTypeName() { return "Dungeons_Inspectable"; }
	static void RegisterScripts(R3::LuaSystem&);
	void SerialiseJson(R3::JsonSerialiser& s);
	void Inspect(const R3::Entities::EntityHandle& e, R3::Entities::World* w, R3::ValueInspector& i);

	std::string m_inspectText = "";		// text to display on inspect
};