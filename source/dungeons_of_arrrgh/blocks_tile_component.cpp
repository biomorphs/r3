#include "blocks_tile_component.h"
#include "engine/systems/lua_system.h"

void DungeonsBlocksTileComponent::RegisterScripts(R3::LuaSystem& l)
{
	R3_PROF_EVENT();
	l.RegisterType<DungeonsBlocksTileComponent>("DungeonsBlocksTileComponent");
}

void DungeonsBlocksTileComponent::SerialiseJson(R3::JsonSerialiser& s)
{
}
