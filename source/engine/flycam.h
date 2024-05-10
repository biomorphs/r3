#pragma once
#include "core/glm_headers.h"

namespace R3
{
	class Camera;
	struct ControllerRawState;
	struct MouseRawState;
	struct KeyboardState;

	// a camera that flies around
	// supports mouse + kb, controller inputs
	// call the updates from fixed update for best results
	class Flycam
	{
	public:
		Flycam();
		virtual ~Flycam();
		void ApplyToCamera(Camera& target);
		void Update(const ControllerRawState& controllerState, double timeDelta);
		void Update(const MouseRawState& mouseState, double timeDelta);
		void Update(const KeyboardState& kbState, double timeDelta);
		void StorePreviousFrameData();
		inline void SetPosition(const glm::vec3& pos) { m_position = pos; }
		inline void SetYaw(float y) { m_yaw = y; }
		inline void SetPitch(float p) { m_pitch = p; }
		inline glm::vec3 GetPosition() const { return m_position; }

		float m_mouseSensitivity = 0.02f;	// keep this tiny
		float m_controllerSensitivity = 2.5f;
		float m_yawRotSpeed = 2.0f;		// radians/s
		float m_pitchRotSpeed = 2.0f;
		float m_forwardSpeed = 2.0f;
		float m_strafeSpeed = 1.8f;
		float m_speedMultiplier = 10.0f;
		float m_highSpeedMultiplier = 50.0f;

	private:
		bool m_mouseLookActive = false;
		glm::vec3 m_position;
		glm::vec3 m_lookDirection;
		glm::vec3 m_right;
		glm::vec3 m_prevFramePosition;
		glm::vec3 m_prevFrameLookDirection;
		float m_pitch;
		float m_yaw;
	};
}