#include "point_light.h"

namespace R3
{
	void PointLightComponent::SerialiseJson(JsonSerialiser& s)
	{
		s("Colour", m_colour);
		s("Distance", m_distance);
		s("Brightness", m_brightness);
		s("Attenuation", m_attenuation);
	}

	void PointLightComponent::Inspect(const Entities::EntityHandle& e, Entities::World* w, ValueInspector& i)
	{
		i.InspectColour("Colour", m_colour, InspectProperty(&PointLightComponent::m_colour, e, w));
		i.Inspect("Brightness", m_brightness, InspectProperty(&PointLightComponent::m_brightness, e, w), 0.0f, 0.1f, 10000.0f);
		i.Inspect("Distance", m_distance, InspectProperty(&PointLightComponent::m_distance, e, w), 0.1f, 0.1f, 10000.0f);
		i.Inspect("Attenuation Factor", m_attenuation, InspectProperty(&PointLightComponent::m_attenuation, e, w), 0.1f, 0.1f, 1000.0f);
	}
}