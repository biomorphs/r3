#include "item_stats_component.h"
#include "engine/systems/lua_system.h"
#include <imgui.h>

void DungeonsItemStatsComponent::RegisterScripts(R3::LuaSystem& l)
{
	R3_PROF_EVENT();
	l.RegisterType<DungeonsItemStatsComponent::DungeonsItemStat>("DungeonsItemStat",
		sol::constructors<DungeonsItemStatsComponent::DungeonsItemStat(std::string_view, int16_t)>(),
		"m_tag", &DungeonsItemStatsComponent::DungeonsItemStat::m_tag,
		"m_value", &DungeonsItemStatsComponent::DungeonsItemStat::m_value
	);
	l.RegisterType<DungeonsItemStatsComponent>("DungeonsItemStats",
		"m_stats", &DungeonsItemStatsComponent::m_stats
	);
}

void DungeonsItemStatsComponent::SerialiseJson(R3::JsonSerialiser& s)
{
	R3_PROF_EVENT();
	s("Stats", m_stats);
}

void DungeonsItemStatsComponent::Inspect(const R3::Entities::EntityHandle& e, R3::Entities::World* w, R3::ValueInspector& i)
{
	R3_PROF_EVENT();
	for (const auto& stat : m_stats)
	{
		auto text = std::format("{}: {}", stat.m_tag.GetString(), stat.m_value);
		ImGui::Text(text.c_str());
	}
}

void DungeonsItemStatsComponent::DungeonsItemStat::SerialiseJson(R3::JsonSerialiser& s)
{
	s("Tag", m_tag);
	s("Value", m_value);
}
