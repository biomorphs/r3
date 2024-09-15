#include "monster_spawn_component.h"
#include "engine/systems/lua_system.h"

void DungeonsMonsterSpawner::RegisterScripts(R3::LuaSystem& l)
{
	R3_PROF_EVENT();
	l.RegisterType<DungeonsMonsterSpawner>("DungeonsMonsterSpawner",
		"m_monsterType", &DungeonsMonsterSpawner::m_monsterType,
		"m_spawnPosition", &DungeonsMonsterSpawner::m_spawnPosition
	);
}

void DungeonsMonsterSpawner::SerialiseJson(R3::JsonSerialiser& s)
{
	R3_PROF_EVENT();
	s("MonsterType", m_monsterType);
	s("Position", m_spawnPosition);
}

void DungeonsMonsterSpawner::Inspect(const R3::Entities::EntityHandle& e, R3::Entities::World* w, R3::ValueInspector& i)
{
	i.Inspect("Type", m_monsterType, R3::InspectProperty(&DungeonsMonsterSpawner::m_monsterType, e, w));
	i.Inspect("Position", m_spawnPosition, R3::InspectProperty(&DungeonsMonsterSpawner::m_spawnPosition, e, w), { 0,0 }, { 1024, 1024 });
}
