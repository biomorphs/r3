#pragma once
#include "entities/component_helpers.h"

namespace R3
{
	class TransformComponent
	{
	public:
		static std::string_view GetTypeName() { return "Transform"; }
		void SerialiseJson(JsonSerialiser& s);
		void Inspect(const Entities::EntityHandle& e, Entities::World* w, ValueInspector& i);

		void SetOrientation(glm::quat q) { m_orientation = glm::normalize(q); RebuildMatrix(); }
		void SetOrientationRadians(glm::vec3 r) { SetOrientation(glm::quat(r)); }
		void SetOrientationDegrees(glm::vec3 rotation) { SetOrientation(glm::quat(glm::radians(rotation))); }
		void SetPosition(glm::vec3 p) { m_position = p; RebuildMatrix(); }
		void SetScale(glm::vec3 s) { m_scale = s; RebuildMatrix(); }

		glm::quat GetOrientation() const { return m_orientation; }
		glm::vec3 GetPosition() const { return m_position; }
		glm::vec3 GetOrientationRadians() const { return glm::eulerAngles(m_orientation); }
		glm::vec3 GetOrientationDegrees() const { return glm::degrees(GetOrientationRadians()); }
		glm::vec3 GetScale() const { return m_scale; }

		glm::mat4 GetWorldspaceMatrix() const;				// this value is probably set in fixed update once every few frames
		glm::mat4 GetWorldspaceInterpolated() const;		// interpolate between the previous + current version, based on fixed delta time remaining
		void StorePreviousFrameData();						// call at start of fixed update!

	private:
		void RebuildMatrix();
		glm::vec3 m_position = { 0.0f,0.0f,0.0f };
		glm::quat m_orientation = glm::identity<glm::quat>();
		glm::vec3 m_scale = { 1.0f,1.0f,1.0f };
		glm::mat4 m_matrix = glm::identity<glm::mat4>();		// local -> parent transform
		glm::vec3 m_prevPosition = { 0.0f,0.0f,0.0f };			// values from previous fixed update
		glm::quat m_prevOrientation = glm::identity<glm::quat>();
		glm::vec3 m_prevScale = { 1.0f,1.0f,1.0f };
	};
}