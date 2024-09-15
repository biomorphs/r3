#include "vision_component.h"
#include "engine/systems/lua_system.h"
#include <imgui.h>

void DungeonsVisionComponent::RegisterScripts(R3::LuaSystem& l)
{
	R3_PROF_EVENT();
	l.RegisterType<DungeonsVisionComponent>("DungeonsVisionComponent",
		"m_visionMaxDistance", &DungeonsVisionComponent::m_visionMaxDistance,
		"m_needsUpdate", &DungeonsVisionComponent::m_needsUpdate,
		"m_visibleTiles", &DungeonsVisionComponent::m_visibleTiles
	);
}

void DungeonsVisionComponent::SerialiseJson(R3::JsonSerialiser& s)
{
	s("MaxDistance", m_visionMaxDistance);
	s("NeedsUpdate", m_needsUpdate);
}

void DungeonsVisionComponent::Inspect(const R3::Entities::EntityHandle& e, R3::Entities::World* w, R3::ValueInspector& i)
{
	i.Inspect("Max Distance", m_visionMaxDistance, R3::InspectProperty(&DungeonsVisionComponent::m_visionMaxDistance, e, w), 0.1f, 0.0f, 1000.0f);
	i.Inspect("Needs Update", m_needsUpdate, R3::InspectProperty(&DungeonsVisionComponent::m_needsUpdate, e, w));
	std::string txt = std::format("{} visible tiles", m_visibleTiles.size());
	ImGui::Text(txt.c_str());
}
