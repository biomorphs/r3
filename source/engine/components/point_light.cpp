#include "point_light.h"
#include "engine/systems/lua_system.h"

namespace R3
{
	void PointLightComponent::RegisterScripts(LuaSystem& l)
	{
		l.RegisterType<PointLightComponent>(GetTypeName(),
			"m_colour", &PointLightComponent::m_colour,
			"m_distance", &PointLightComponent::m_distance,
			"m_brightness", &PointLightComponent::m_brightness
		);
	}

	void PointLightComponent::SerialiseJson(JsonSerialiser& s)
	{
		s("Colour", m_colour);
		s("Distance", m_distance);
		s("Brightness", m_brightness);
	}

	void PointLightComponent::Inspect(const Entities::EntityHandle& e, Entities::World* w, ValueInspector& i)
	{
		i.InspectColour("Colour", m_colour, InspectProperty(&PointLightComponent::m_colour, e, w));
		i.Inspect("Brightness", m_brightness, InspectProperty(&PointLightComponent::m_brightness, e, w), 0.0f, 0.1f, 10000.0f);
		i.Inspect("Distance", m_distance, InspectProperty(&PointLightComponent::m_distance, e, w), 0.1f, 0.1f, 10000.0f);
	}
}