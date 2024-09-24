#include "inventory_component.h"
#include "engine/systems/lua_system.h"
#include <imgui.h>

void DungeonsInventoryComponent::RegisterScripts(R3::LuaSystem& l)
{
	R3_PROF_EVENT();
	l.RegisterType<DungeonsInventoryComponent>("DungeonsInventoryComponent",
		"m_allItems", &DungeonsInventoryComponent::m_allItems
	);
}

void DungeonsInventoryComponent::SerialiseJson(R3::JsonSerialiser& s)
{
	R3_PROF_EVENT();
	s("Items", m_allItems);
}

void DungeonsInventoryComponent::Inspect(const R3::Entities::EntityHandle& e, R3::Entities::World* w, R3::ValueInspector& i)
{
	R3_PROF_EVENT();
	for (int c = 0; c < m_allItems.size(); ++c)
	{
		ImGui::Text(w->GetEntityDisplayName(m_allItems[c]).c_str());
	}
}
