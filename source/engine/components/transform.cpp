#include "transform.h"
#include "engine/systems/time_system.h"
#include "engine/systems/lua_system.h"

namespace R3
{
	void TransformComponent::RegisterScripts(LuaSystem& l)
	{
		l.RegisterType<TransformComponent>(GetTypeName(),
			"SetPosition", &TransformComponent::SetPosition,
			"GetPosition", &TransformComponent::GetPosition,
			"SetOrientation", &TransformComponent::SetOrientation,
			"GetOrientation", &TransformComponent::GetOrientation,
			"SetScale", &TransformComponent::SetScale,
			"GetScale", &TransformComponent::GetScale,
			"GetWorldspaceMatrix", &TransformComponent::GetWorldspaceMatrix,
			"GetWorldspaceInterpolated", &TransformComponent::GetWorldspaceInterpolated
		);
	}

	void TransformComponent::SerialiseJson(JsonSerialiser& s)
	{
		s("Position", m_position);
		s("Orientation", m_orientation);
		s("Scale", m_scale);
		if (s.GetMode() == JsonSerialiser::Read)
		{
			RebuildMatrix();
			StorePreviousFrameData();		// just copy the new transform
		}
	}

	void TransformComponent::Inspect(const Entities::EntityHandle& e, Entities::World* w, ValueInspector& i)
	{
		glm::vec3 maxBounds(FLT_MAX);
		bool modified = false;
		modified |= i.Inspect("Position", m_position, InspectProperty(&TransformComponent::SetPosition, e, w), -maxBounds, maxBounds);
		modified |= i.Inspect("Rotation", GetOrientationDegrees(), InspectProperty(&TransformComponent::SetOrientationDegrees, e, w), -maxBounds, maxBounds);
		modified |= i.Inspect("Scale", m_scale, InspectProperty(&TransformComponent::SetScale, e, w), { 0,0,0 }, maxBounds);
	}

	glm::mat4 TransformComponent::GetWorldspaceMatrix() const
	{
		return m_matrix;	// later on we will want some kind of heirarchy
	}

	glm::mat4 TransformComponent::GetWorldspaceInterpolated() const
	{
		glm::mat4 result;
		if (m_prevPosition == m_position && m_prevScale == m_scale && m_prevOrientation == m_orientation)
		{
			result = m_matrix;
		}
		else
		{
			static auto time = Systems::GetSystem<TimeSystem>();
			double accumulator = time->GetFixedUpdateInterpolation();
			const glm::vec3 pos = glm::mix(m_prevPosition, m_position, accumulator);
			const glm::vec3 scale = glm::mix(m_prevScale, m_scale, accumulator);
			const glm::quat rot = glm::slerp(m_prevOrientation, m_orientation, static_cast<float>(accumulator));
			result = glm::scale(glm::translate(glm::identity<glm::mat4>(), pos) * glm::mat4_cast(rot), scale);
		}
		return result;
	}

	void TransformComponent::StorePreviousFrameData()
	{
		m_prevPosition = m_position;
		m_prevOrientation = m_orientation;
		m_prevScale = m_scale;
	}

	void TransformComponent::RebuildMatrix()
	{
		glm::mat4 modelMat = glm::translate(glm::identity<glm::mat4>(), m_position);
		glm::mat4 rotation = glm::toMat4(m_orientation);
		modelMat = modelMat * rotation;
		m_matrix = glm::scale(modelMat, m_scale);
	}
}