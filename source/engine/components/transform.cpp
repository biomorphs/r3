#include "transform.h"

namespace R3
{
	void TransformComponent::SerialiseJson(JsonSerialiser& s)
	{
		s("Position", m_position);
		s("Orientation", m_orientation);
		s("Scale", m_scale);
		if (s.GetMode() == JsonSerialiser::Read)
		{
			RebuildMatrix();
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

	void TransformComponent::RebuildMatrix()
	{
		glm::mat4 modelMat = glm::translate(glm::identity<glm::mat4>(), m_position);
		glm::mat4 rotation = glm::toMat4(m_orientation);
		modelMat = modelMat * rotation;
		m_matrix = glm::scale(modelMat, m_scale);
	}
}