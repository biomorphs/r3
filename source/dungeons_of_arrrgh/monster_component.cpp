#include "monster_component.h"
#include "engine/systems/lua_system.h"

void DungeonsMonsterComponent::RegisterScripts(R3::LuaSystem& l)
{
	R3_PROF_EVENT();
	l.RegisterType<DungeonsMonsterComponent>("DungeonsMonster",
		"m_name", &DungeonsMonsterComponent::m_name
	);
}

void DungeonsMonsterComponent::SerialiseJson(R3::JsonSerialiser& s)
{
	R3_PROF_EVENT();
	s("Name", m_name);
}

void DungeonsMonsterComponent::Inspect(const R3::Entities::EntityHandle& e, R3::Entities::World* w, R3::ValueInspector& i)
{
	i.Inspect("Name", m_name, R3::InspectProperty(&DungeonsMonsterComponent::m_name, e, w));
}
