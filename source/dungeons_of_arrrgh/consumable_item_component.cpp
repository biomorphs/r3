#include "consumable_item_component.h"
#include "engine/systems/lua_system.h"
#include <imgui.h>

void DungeonsConsumableItemComponent::RegisterScripts(R3::LuaSystem& l)
{
	R3_PROF_EVENT();
	l.RegisterType<DungeonsConsumableItemComponent>("DungeonsConsumableItemComponent",
		"m_hpOnUse", &DungeonsConsumableItemComponent::m_hpOnUse,
		"SetIsFood", &DungeonsConsumableItemComponent::SetIsFood,
		"SetIsDrink", &DungeonsConsumableItemComponent::SetIsDrink,
		"IsFood", &DungeonsConsumableItemComponent::IsFood,
		"IsDrink", &DungeonsConsumableItemComponent::IsDrink
	);
}

void DungeonsConsumableItemComponent::SerialiseJson(R3::JsonSerialiser& s)
{
	R3_PROF_EVENT();
	s("HPOnUse", m_hpOnUse);
	s("ConsumeType", (int32_t&)m_type);
}

void DungeonsConsumableItemComponent::Inspect(const R3::Entities::EntityHandle& e, R3::Entities::World* w, R3::ValueInspector& i)
{
	R3_PROF_EVENT();
	i.Inspect("HP On Use", m_hpOnUse, R3::InspectProperty(&DungeonsConsumableItemComponent::m_hpOnUse, e, w), 1, -10000,10000);
	i.Inspect("Is Food?", IsFood(), R3::InspectProperty(&DungeonsConsumableItemComponent::SetIsFood, e, w));
	i.Inspect("Is Drink?", IsDrink(), R3::InspectProperty(&DungeonsConsumableItemComponent::SetIsDrink, e, w));
}
