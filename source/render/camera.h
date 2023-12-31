#pragma once

#include "core/glm_headers.h"

namespace R3
{
	class Camera
	{
	public:
		Camera();
		~Camera();
		void SetProjection(float fov, float aspect, float nearPlane, float farPlane);
		void SetFOVAndAspectRatio(float fov, float aspect);
		void SetClipPlanes(float nearPlane, float farPlane);
		void SetPosition(const glm::vec3& pos);
		void SetUp(const glm::vec3& up);
		void SetTarget(const glm::vec3& target);
		void LookAt(const glm::vec3& pos, const glm::vec3& target, const glm::vec3& up);
		void LookAt(const glm::vec3& target, const glm::vec3& up);
		void LookAt(const glm::vec3& target);

		// 0,0 = bottom left
		inline glm::vec3 WindowPositionToWorldSpace(glm::vec2 p, glm::vec2 windowSize) const;

		inline const glm::vec3& Position() const			{ return m_position; }
		inline const glm::vec3& Up() const					{ return m_up; }
		inline const glm::vec3& Target() const				{ return m_target; }
		inline const glm::mat4& ViewMatrix() const			{ return m_viewMatrix; }
		inline const glm::mat4& ProjectionMatrix() const	{ return m_projectionMatrix; }
		inline float NearPlane() const						{ return m_nearPlane; }
		inline float FarPlane() const						{ return m_farPlane; }
		inline float FOV() const							{ return m_fov; }

	private:
		void RebuildViewMatrix();
		void RebuildProjectionMatrix();

		glm::mat4 m_viewMatrix;
		glm::mat4 m_projectionMatrix;
		glm::vec3 m_position;
		glm::vec3 m_up;
		glm::vec3 m_target;
		float m_nearPlane;
		float m_farPlane;
		float m_fov;
		float m_aspect;
	};
}

#include "camera.inl"