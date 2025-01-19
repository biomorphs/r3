#pragma once

#include "entities/component_helpers.h"
#include "engine/utils/tag.h"
#include <unordered_map>

class DungeonsEquippedItemsComponent
{
public:
	static std::string_view GetTypeName() { return "Dungeons_EquippedItems"; }
	static void RegisterScripts(R3::LuaSystem&);
	void SerialiseJson(R3::JsonSerialiser& s);
	void Inspect(const R3::Entities::EntityHandle& e, R3::Entities::World* w, R3::ValueInspector& i);
	void AddSlot(R3::Tag slotName);

	std::unordered_map<R3::Tag, R3::Entities::EntityHandle> m_slots;	// 1 item / slot
};