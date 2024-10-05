#include "wearable_item_component.h"
#include "engine/systems/lua_system.h"
#include <imgui.h>

void DungeonsWearableItemComponent::RegisterScripts(R3::LuaSystem& l)
{
	R3_PROF_EVENT();
	l.RegisterType<DungeonsWearableItemComponent>("DungeonsWearableItemComponent",
		"m_slot", &DungeonsWearableItemComponent::m_slot,
		"m_onEquipFn", &DungeonsWearableItemComponent::m_onEquipFn,
		"m_onRemoveFn", &DungeonsWearableItemComponent::m_onRemoveFn
	);
}

void DungeonsWearableItemComponent::SerialiseJson(R3::JsonSerialiser& s)
{
	R3_PROF_EVENT();
	s("Slot", m_slot);
	s("OnEquip", m_onEquipFn);
	s("OnRemove", m_onRemoveFn);
}

void DungeonsWearableItemComponent::Inspect(const R3::Entities::EntityHandle& e, R3::Entities::World* w, R3::ValueInspector& i)
{
	R3_PROF_EVENT();
	i.Inspect("Slot", m_slot, R3::InspectProperty(&DungeonsWearableItemComponent::m_slot, e, w));
	i.Inspect("OnEquip", m_onEquipFn, R3::InspectProperty(&DungeonsWearableItemComponent::m_onEquipFn, e, w));
	i.Inspect("OnRemove", m_onRemoveFn, R3::InspectProperty(&DungeonsWearableItemComponent::m_onRemoveFn, e, w));
}
