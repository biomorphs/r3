#include "camera.h"
#include "engine/systems/lua_system.h"

namespace R3
{
	void CameraComponent::RegisterScripts(class LuaSystem& l)
	{
		l.RegisterType<CameraComponent>(GetTypeName(),
			"m_nearPlane", &CameraComponent::m_nearPlane,
			"m_farPlane", &CameraComponent::m_farPlane,
			"m_fov", &CameraComponent::m_fov
		);
	}

	void CameraComponent::SerialiseJson(JsonSerialiser& s)
	{
		s("Near Plane", m_nearPlane);
		s("Far Plane", m_farPlane);
		s("FOV", m_fov);
	}

	void CameraComponent::Inspect(const Entities::EntityHandle& e, Entities::World* w, ValueInspector& i)
	{
		i.Inspect("FOV", m_fov, InspectProperty(&CameraComponent::m_fov, e, w), 0.1f, 0.1f, 180.0f);
		i.Inspect("Near Plane", m_nearPlane, InspectProperty(&CameraComponent::m_nearPlane, e, w), 0.1f, 0.0f, m_farPlane);
		i.Inspect("Far Plane", m_farPlane, InspectProperty(&CameraComponent::m_farPlane, e, w), 0.1f, m_nearPlane, 10000000.0f);
	}
}