#pragma once

#include "entities/component_helpers.h"
#include "engine/tag.h"

// A wearable item that can be equipped in a named slot
class DungeonsWearableItemComponent
{
public:
	static std::string_view GetTypeName() { return "Dungeons_WearableItem"; }
	static void RegisterScripts(R3::LuaSystem&);
	void SerialiseJson(R3::JsonSerialiser& s);
	void Inspect(const R3::Entities::EntityHandle& e, R3::Entities::World* w, R3::ValueInspector& i);

	R3::Tag m_slot;	// equipped items must match a slot in a equipped_items component
};