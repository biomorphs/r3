#include "item_component.h"
#include "engine/systems/lua_system.h"
#include <imgui.h>

void DungeonsItemComponent::RegisterScripts(R3::LuaSystem& l)
{
	R3_PROF_EVENT();
	l.RegisterType<DungeonsItemComponent>("DungeonsItemComponent",
		"m_name", &DungeonsItemComponent::m_name
	);
}

void DungeonsItemComponent::SerialiseJson(R3::JsonSerialiser& s)
{
	R3_PROF_EVENT();
	s("Name", m_name);
}

void DungeonsItemComponent::Inspect(const R3::Entities::EntityHandle& e, R3::Entities::World* w, R3::ValueInspector& i)
{
	R3_PROF_EVENT();
	i.Inspect("Name", m_name, R3::InspectProperty(&DungeonsItemComponent::m_name, e, w));
}
