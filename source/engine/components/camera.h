#pragma once
#include "entities/component_helpers.h"

namespace R3
{
	class CameraComponent
	{
	public:
		static std::string_view GetTypeName() { return "Camera"; }
		void SerialiseJson(JsonSerialiser& s);
		void Inspect(const Entities::EntityHandle& e, Entities::World* w, ValueInspector& i);

		float m_nearPlane = 0.1f;
		float m_farPlane = 5000.0f;
		float m_fov = 70.0f;
	};
}