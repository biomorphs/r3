#pragma once

#include "entities/component_helpers.h"

// Consumables can be eaten/drank and affect the consumer in various ways
class DungeonsConsumableItemComponent
{
public:
	static std::string_view GetTypeName() { return "Dungeons_ConsumableItem"; }
	static void RegisterScripts(R3::LuaSystem&);
	void SerialiseJson(R3::JsonSerialiser& s);
	void Inspect(const R3::Entities::EntityHandle& e, R3::Entities::World* w, R3::ValueInspector& i);

	enum ConsumableType {
		Food = 0,
		Drink = 1
	};
	void SetIsFood(bool) { m_type = ConsumableType::Food; }	// for scripts, avoiding direct enum use, ignored param is for gui
	void SetIsDrink(bool) { m_type = ConsumableType::Drink; }
	bool IsFood() { return m_type == ConsumableType::Food; }
	bool IsDrink() { return m_type == ConsumableType::Drink; }
	
	int m_hpOnUse = 0;	// set to >0 to heal on use, <0 to damage user on use
	ConsumableType m_type = ConsumableType::Food;
};