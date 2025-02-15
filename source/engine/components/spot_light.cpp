#include "spot_light.h"
#include "engine/systems/lua_system.h"

namespace R3
{
	void SpotLightComponent::RegisterScripts(LuaSystem& l)
	{
		l.RegisterType<SpotLightComponent>(GetTypeName(),
			"m_colour", &SpotLightComponent::m_colour,
			"m_distance", &SpotLightComponent::m_distance,
			"m_outerAngle", &SpotLightComponent::m_outerAngle,
			"m_innerAngle", &SpotLightComponent::m_innerAngle,
			"m_brightness", &SpotLightComponent::m_brightness,
			"m_enabled", &SpotLightComponent::m_enabled
		);
	}

	void SpotLightComponent::SerialiseJson(JsonSerialiser& s)
	{
		s("Colour", m_colour);
		s("Distance", m_distance);
		s("OuterAngle", m_outerAngle);
		s("InnerAngle", m_innerAngle);
		s("Brightness", m_brightness);
		s("Enabled", m_enabled);
	}

	void SpotLightComponent::Inspect(const Entities::EntityHandle& e, Entities::World* w, ValueInspector& i)
	{
		i.InspectColour("Colour", m_colour, InspectProperty(&SpotLightComponent::m_colour, e, w));
		i.Inspect("Brightness", m_brightness, InspectProperty(&SpotLightComponent::m_brightness, e, w), 0.0f, 0.1f, 10000.0f);
		i.Inspect("Distance", m_distance, InspectProperty(&SpotLightComponent::m_distance, e, w), 0.1f, 0.1f, 10000.0f);
		i.Inspect("Outer Angle (Degrees)", m_outerAngle, InspectProperty(&SpotLightComponent::m_outerAngle, e, w), 0.1f, glm::max(0.1f,m_innerAngle), 90.0f);
		i.Inspect("Inner Angle (Degrees)", m_innerAngle, InspectProperty(&SpotLightComponent::m_innerAngle, e, w), 0.1f, 0.1f, glm::min(m_outerAngle, 90.0f));
		i.Inspect("Enabled", m_enabled, InspectProperty(&SpotLightComponent::m_enabled, e, w));
	}
}