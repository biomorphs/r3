#pragma once

#include "entities/component_helpers.h"
#include "engine/tag.h"

// contains all the stats/values associated with an item
class DungeonsItemStatsComponent
{
public:
	static std::string_view GetTypeName() { return "Dungeons_ItemStats"; }
	static void RegisterScripts(R3::LuaSystem&);
	void SerialiseJson(R3::JsonSerialiser& s);
	void Inspect(const R3::Entities::EntityHandle& e, R3::Entities::World* w, R3::ValueInspector& i);

	struct DungeonsItemStat
	{
		DungeonsItemStat() = default;
		DungeonsItemStat(std::string_view t, int16_t v) 
			: m_tag(t), m_value(v) { }

		R3::Tag m_tag;
		int16_t m_value = 0;
		void SerialiseJson(R3::JsonSerialiser& s);
	};
	std::vector<DungeonsItemStat> m_stats;
};