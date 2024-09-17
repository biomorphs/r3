#include "inspectable_component.h"
#include "engine/systems/lua_system.h"

void DungeonsInspectableComponent::RegisterScripts(R3::LuaSystem& l)
{
	R3_PROF_EVENT();
	l.RegisterType<DungeonsInspectableComponent>("DungeonsInspectableComponent",
		"m_inspectText", &DungeonsInspectableComponent::m_inspectText
	);
}

void DungeonsInspectableComponent::SerialiseJson(R3::JsonSerialiser& s)
{
	R3_PROF_EVENT();
	s("InspectText", m_inspectText);
}

void DungeonsInspectableComponent::Inspect(const R3::Entities::EntityHandle& e, R3::Entities::World* w, R3::ValueInspector& i)
{
	R3_PROF_EVENT();
	i.Inspect("Inspect Text", m_inspectText, R3::InspectProperty(&DungeonsInspectableComponent::m_inspectText, e, w));
}
