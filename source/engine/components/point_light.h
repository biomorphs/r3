#pragma once
#include "entities/component_helpers.h"

namespace R3
{
	class PointLightComponent
	{
	public:
		static std::string_view GetTypeName() { return "Point Light"; }
		void SerialiseJson(JsonSerialiser& s);
		void Inspect(const Entities::EntityHandle& e, Entities::World* w, ValueInspector& i);

		glm::vec3 m_colour = { 1,1,1 };
		float m_distance = 8.0f;		// max distance it can cast light
		float m_brightness = 1.0f;
		float m_attenuation = 2.0f;		// adjusts falloff curve
	};
}