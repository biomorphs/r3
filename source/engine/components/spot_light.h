#pragma once
#include "entities/component_helpers.h"

namespace R3
{
	// A spotlight that points towards +z
	class SpotLightComponent 
	{
	public:
		static std::string_view GetTypeName() { return "SpotLight"; }
		static void RegisterScripts(class LuaSystem&);
		void SerialiseJson(JsonSerialiser& s);
		void Inspect(const Entities::EntityHandle& e, Entities::World* w, ValueInspector& i);

		glm::vec3 m_colour = { 1,1,1 };
		float m_distance = 8.0f;		// max distance it can cast light
		float m_outerAngle = 45.0f;		// outer angle of spotlight cone
		float m_innerAngle = 10.0f;		// inner angle controls bright spot in the cone
		float m_brightness = 1.0f;
		bool m_enabled = true;
	};
}