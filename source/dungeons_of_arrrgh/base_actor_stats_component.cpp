#include "base_actor_stats_component.h"
#include "engine/systems/lua_system.h"
#include <imgui.h>

void DungeonsBaseActorStatsComponent::RegisterScripts(R3::LuaSystem& l)
{
	R3_PROF_EVENT();
	l.RegisterType<DungeonsBaseActorStatsComponent>("DungeonsBaseActorStatsComponent",
		"m_level", &DungeonsBaseActorStatsComponent::m_level,
		"m_baseMaxHP", &DungeonsBaseActorStatsComponent::m_baseMaxHP,
		"m_strength", &DungeonsBaseActorStatsComponent::m_strength,
		"m_endurance", &DungeonsBaseActorStatsComponent::m_endurance,
		"m_currentHP", &DungeonsBaseActorStatsComponent::m_currentHP
	);
}

void DungeonsBaseActorStatsComponent::SerialiseJson(R3::JsonSerialiser& s)
{
	R3_PROF_EVENT();
	s("Level", m_level);
	s("BaseMaxHP", m_baseMaxHP);
	s("Strength", m_strength);
	s("Endurance", m_endurance);
	s("CurrentHP", m_currentHP);
}

void DungeonsBaseActorStatsComponent::Inspect(const R3::Entities::EntityHandle& e, R3::Entities::World* w, R3::ValueInspector& i)
{
	R3_PROF_EVENT();
	i.Inspect("Level", m_level, R3::InspectProperty(&DungeonsBaseActorStatsComponent::m_level, e, w), 1, 1, 1000);
	ImGui::Separator();
	i.Inspect("CurrentHP", m_currentHP, R3::InspectProperty(&DungeonsBaseActorStatsComponent::m_currentHP, e, w), 1, 0, 1000);
	i.Inspect("Base Max HP", m_baseMaxHP, R3::InspectProperty(&DungeonsBaseActorStatsComponent::m_baseMaxHP, e, w), 1, 1, 1000);
	ImGui::Separator();
	i.Inspect("Strength", m_strength, R3::InspectProperty(&DungeonsBaseActorStatsComponent::m_strength, e, w), 1, 0, 1000);
	i.Inspect("Endurance", m_endurance, R3::InspectProperty(&DungeonsBaseActorStatsComponent::m_endurance, e, w), 1, 0, 1000);
}
