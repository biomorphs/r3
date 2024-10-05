#pragma once

#include "entities/component_helpers.h"

// An item with a name. items can be added to item containers
// All callbacks should have the signature fn(actor entity, item entity)
class DungeonsItemComponent
{
public:
	static std::string_view GetTypeName() { return "Dungeons_Item"; }
	static void RegisterScripts(R3::LuaSystem&);
	void SerialiseJson(R3::JsonSerialiser& s);
	void Inspect(const R3::Entities::EntityHandle& e, R3::Entities::World* w, R3::ValueInspector& i);

	std::string m_name;
	std::string m_onPickupFn;
};