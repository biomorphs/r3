#include "equipped_items_component.h"
#include "engine/systems/lua_system.h"
#include <imgui.h>

void DungeonsEquippedItemsComponent::RegisterScripts(R3::LuaSystem& l)
{
	R3_PROF_EVENT();
	l.RegisterType<DungeonsEquippedItemsComponent>("DungeonsEquippedItemsComponent",
		"m_slots", &DungeonsEquippedItemsComponent::m_slots,
		"AddSlot", &DungeonsEquippedItemsComponent::AddSlot
	);
}

void DungeonsEquippedItemsComponent::SerialiseJson(R3::JsonSerialiser& s)
{
	R3_PROF_EVENT();
	s("Slots", m_slots);
}

void DungeonsEquippedItemsComponent::Inspect(const R3::Entities::EntityHandle& e, R3::Entities::World* w, R3::ValueInspector& i)
{
	R3_PROF_EVENT();
	for (auto& s : m_slots)
	{
		std::string text = std::format("{}: {}", s.first.GetString(), w->GetEntityName(s.second));
		ImGui::Text(text.c_str());
	}
}

void DungeonsEquippedItemsComponent::AddSlot(R3::Tag slotName)
{
	m_slots[slotName] = {};
}