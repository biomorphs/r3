#pragma once

#include "entities/component_helpers.h"

// Inventory is a list of item entity references
// Later on, items can be containers of other items
class DungeonsInventoryComponent
{
public:
	static std::string_view GetTypeName() { return "Dungeons_Inventory"; }
	static void RegisterScripts(R3::LuaSystem&);
	void SerialiseJson(R3::JsonSerialiser& s);
	void Inspect(const R3::Entities::EntityHandle& e, R3::Entities::World* w, R3::ValueInspector& i);

	std::vector<R3::Entities::EntityHandle> m_allItems;
};