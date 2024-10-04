#pragma once

#include "entities/component_helpers.h"

// represents base stats shared by all 'living' actors
// level, hp, mana, stamina, base combat stats, etc
class DungeonsBaseActorStatsComponent
{
public:
	static std::string_view GetTypeName() { return "Dungeons_BaseActorStats"; }
	static void RegisterScripts(R3::LuaSystem&);
	void SerialiseJson(R3::JsonSerialiser& s);
	void Inspect(const R3::Entities::EntityHandle& e, R3::Entities::World* w, R3::ValueInspector& i);

	// Use these accessors always as they perform all the relevant scaling calculations
	int32_t GetMaxHP() const;

	// base stats (only change on level up)
	int32_t m_level = 1;
	int32_t m_baseMaxHP = 1;
	
	// combat stats
	int32_t m_strength = 0;			// affects melee damage
	int32_t m_endurance = 0;		// affects max hp
	int32_t m_baseHitChance = 0;	// chance to hit (%)

	// dynamic stats (change during regular gameplay)
	int32_t m_currentHP = 0;
};