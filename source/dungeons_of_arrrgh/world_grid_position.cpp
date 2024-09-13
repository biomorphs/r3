#include "world_grid_position.h"
#include "engine/systems/lua_system.h"

void DungeonsWorldGridPosition::RegisterScripts(R3::LuaSystem& l)
{
	l.RegisterType<DungeonsWorldGridPosition>("WorldGridPosition",
		"GetPosition", &DungeonsWorldGridPosition::GetPosition
	);
}

void DungeonsWorldGridPosition::SerialiseJson(R3::JsonSerialiser& s)
{
	s("Position", m_position);
}

void DungeonsWorldGridPosition::Inspect(const R3::Entities::EntityHandle& e, R3::Entities::World* w, R3::ValueInspector& i)
{
	i.Inspect("Position", m_position, [](glm::uvec2) {}, { 0,0 }, { -1,-1 });	// read-only
}
