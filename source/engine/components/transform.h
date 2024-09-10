#pragma once
#include "entities/component_helpers.h"

namespace R3
{
	class TransformComponent
	{
	public:
		static std::string_view GetTypeName() { return "Transform"; }
		static void RegisterScripts(class LuaSystem&);
		void SerialiseJson(JsonSerialiser& s);
		void Inspect(const Entities::EntityHandle& e, Entities::World* w, ValueInspector& i);

		void SetOrientation(glm::quat q) { m_orientation = glm::normalize(q); RebuildMatrix(); }
		void SetOrientationNoInterpolation(glm::quat q) { SetOrientation(q); m_prevOrientation = m_orientation; }
		void SetOrientationRadians(glm::vec3 r) { SetOrientation(glm::quat(r)); }
		void SetOrientationDegrees(glm::vec3 rotation) { SetOrientation(glm::quat(glm::radians(rotation))); }
		void SetPosition(glm::vec3 p) { m_position = p; RebuildMatrix(); }
		void SetPositionNoInterpolation(glm::vec3 p) { SetPosition(p); m_prevPosition = m_position; }
		void SetScale(glm::vec3 s) { m_scale = s; RebuildMatrix(); }
		void SetScaleNoInterpolation(glm::vec3 p) { SetScale(p); m_prevScale = m_scale; }

		glm::quat GetOrientation() const { return m_orientation; }
		glm::vec3 GetPosition() const { return m_position; }
		glm::vec3 GetOrientationRadians() const { return glm::eulerAngles(m_orientation); }
		glm::vec3 GetOrientationDegrees() const { return glm::degrees(GetOrientationRadians()); }
		glm::vec3 GetScale() const { return m_scale; }

		// In order to support transform heirarchies, these functions need the owner entity + world
		bool IsRelativeToParent() const { return m_isRelative; }
		void SetIsRelative(bool r) { m_isRelative = r; }
		void SetPositionWorldSpace(const Entities::EntityHandle& e, Entities::World& w, glm::vec3 p);				// position will be recalculated relative to parent
		void SetPositionWorldSpaceNoInterpolation(const Entities::EntityHandle& e, Entities::World& w, glm::vec3 p);
		glm::mat4 GetWorldspaceMatrix(const Entities::EntityHandle& e, Entities::World& w) const;				// no interpolation, always returns the latest value
		glm::mat4 GetWorldspaceInterpolated(const Entities::EntityHandle& e, Entities::World& w) const;			// interpolate between the previous + current version, based on fixed delta time remaining

		void StorePreviousFrameData();						// called at start of fixed update, used to interpolate values! should not be public API

	private:
		void RebuildMatrix();
		glm::mat4 m_matrix = glm::identity<glm::mat4>();		// local -> parent transform
		glm::vec3 m_position = { 0.0f,0.0f,0.0f };
		glm::quat m_orientation = glm::identity<glm::quat>();
		glm::vec3 m_scale = { 1.0f,1.0f,1.0f };
		glm::vec3 m_prevPosition = { 0.0f,0.0f,0.0f };			// values from previous fixed update
		glm::quat m_prevOrientation = glm::identity<glm::quat>();
		glm::vec3 m_prevScale = { 1.0f,1.0f,1.0f };
		bool m_isRelative = false;							// if true, parent transform comes from entity heirarchy
	};
}