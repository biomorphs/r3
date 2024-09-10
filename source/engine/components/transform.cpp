#include "transform.h"
#include "engine/systems/time_system.h"
#include "engine/systems/lua_system.h"

namespace R3
{
	void TransformComponent::RegisterScripts(LuaSystem& l)
	{
		l.RegisterType<TransformComponent>(GetTypeName(),
			"SetPosition", &TransformComponent::SetPosition,
			"SetPositionNoInterpolation", &TransformComponent::SetPositionNoInterpolation,
			"GetPosition", &TransformComponent::GetPosition,
			"SetOrientation", &TransformComponent::SetOrientation,
			"SetOrientationNoInterpolation", &TransformComponent::SetOrientationNoInterpolation,
			"GetOrientation", &TransformComponent::GetOrientation,
			"SetScale", &TransformComponent::SetScale,
			"SetScaleNoInterpolation", &TransformComponent::SetScaleNoInterpolation,
			"GetScale", &TransformComponent::GetScale,
			"GetWorldspaceMatrix", &TransformComponent::GetWorldspaceMatrix,
			"GetWorldspaceInterpolated", &TransformComponent::GetWorldspaceInterpolated,
			"IsRelativeToParent", &TransformComponent::IsRelativeToParent,
			"SetIsRelative", &TransformComponent::SetIsRelative
		);
	}

	void TransformComponent::SerialiseJson(JsonSerialiser& s)
	{
		s("Position", m_position);
		s("Orientation", m_orientation);
		s("Scale", m_scale);
		s("Relative", m_isRelative);
		if (s.GetMode() == JsonSerialiser::Read)
		{
			RebuildMatrix();
			StorePreviousFrameData();		// just copy the new transform
		}
	}

	void TransformComponent::Inspect(const Entities::EntityHandle& e, Entities::World* w, ValueInspector& i)
	{
		glm::vec3 maxBounds(FLT_MAX);
		i.Inspect("Position", m_position, InspectProperty(&TransformComponent::SetPosition, e, w), -maxBounds, maxBounds);
		i.Inspect("Rotation", GetOrientationDegrees(), InspectProperty(&TransformComponent::SetOrientationDegrees, e, w), -maxBounds, maxBounds);
		i.Inspect("Scale", m_scale, InspectProperty(&TransformComponent::SetScale, e, w), { 0,0,0 }, maxBounds);
		i.Inspect("Is Relative to Parent", m_isRelative, InspectProperty(&TransformComponent::m_isRelative, e, w));
	}

	void TransformComponent::SetPositionWorldSpaceNoInterpolation(const Entities::EntityHandle& e, Entities::World& w, glm::vec3 p)
	{
		// worldspace -> local space via parent inverse transform
		if (m_isRelative)
		{
			glm::mat4 parentMatrix = glm::identity<glm::mat4>();
			auto parentEntity = w.GetParent(e);
			auto parentTransform = w.GetComponent<TransformComponent>(parentEntity);
			if (parentTransform)
			{
				parentMatrix = parentTransform->GetWorldspaceMatrix(parentEntity, w);	// recursive
			}
			glm::vec4 newPos = glm::inverse(parentMatrix) * glm::vec4(p, 1);
			SetPositionNoInterpolation(glm::vec3(newPos));
		}
		else
		{
			SetPositionNoInterpolation(p);
		}
	}

	void TransformComponent::SetPositionWorldSpace(const Entities::EntityHandle& e, Entities::World& w, glm::vec3 p)
	{
		// worldspace -> local space via parent inverse transform
		if (m_isRelative)
		{
			glm::mat4 parentMatrix = glm::identity<glm::mat4>();
			auto parentEntity = w.GetParent(e);
			auto parentTransform = w.GetComponent<TransformComponent>(parentEntity);
			if (parentTransform)
			{
				parentMatrix = parentTransform->GetWorldspaceMatrix(parentEntity, w);	// recursive
			}
			glm::vec4 newPos = glm::inverse(parentMatrix) * glm::vec4(p,1);
			SetPosition(glm::vec3(newPos));
		}
		else
		{
			SetPosition(p);
		}
	}

	glm::mat4 TransformComponent::GetWorldspaceMatrix(const Entities::EntityHandle& e, Entities::World& w) const
	{
		if (m_isRelative)
		{
			glm::mat4 parentMatrix = glm::identity<glm::mat4>();
			auto parentEntity = w.GetParent(e);
			auto parentTransform = w.GetComponent<TransformComponent>(parentEntity);
			if (parentTransform)
			{
				parentMatrix = parentTransform->GetWorldspaceMatrix(parentEntity, w);	// recursive
			}
			return parentMatrix * m_matrix;
		}
		else
		{
			return m_matrix;
		}
	}

	glm::mat4 TransformComponent::GetWorldspaceInterpolated(const Entities::EntityHandle& e, Entities::World& w) const
	{
		glm::mat4 result;

		// first calculate interpolated matrix for this transform
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

		// apply parent if required
		if (m_isRelative)
		{
			auto parentEntity = w.GetParent(e);
			auto parentTransform = w.GetComponent<TransformComponent>(parentEntity);
			if (parentTransform)
			{
				result = parentTransform->GetWorldspaceInterpolated(parentEntity, w) * result;
			}
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