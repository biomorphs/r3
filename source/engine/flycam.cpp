#include "flycam.h"
#include "engine/systems/input_system.h"
#include "render/camera.h"
#include "core/profiler.h"

namespace R3
{
	const float c_maxPitchValue = (glm::pi<float>() * 0.5f) - 0.01f;	// just < 90 degrees to avoid flippage

	Flycam::Flycam()
		: m_position(0.0f, 0.0f, 0.0f)
		, m_pitch(0.0f)
		, m_yaw(0.0f)
	{

	}

	Flycam::~Flycam()
	{

	}

	void Flycam::ApplyToCamera(Camera& target)
	{
		glm::vec3 up(0.0f, 1.0f, 0.0f);
		target.LookAt(m_position, m_position + m_lookDirection, up);
	}

	void Flycam::Update(const MouseRawState& mouse, double timeDelta)
	{
		R3_PROF_EVENT();

		const auto mousePosition = glm::ivec2(mouse.m_cursorX, mouse.m_cursorY);
		const float timeDeltaF = (float)timeDelta;
		static bool enabled = false;
		static glm::ivec2 lastClickPos = { 0,0 };
		
		if (mouse.m_buttonState & MouseButtons::LeftButton)
		{
			if (!enabled)
			{
				lastClickPos = mousePosition;
				enabled = true;
			}
			else
			{
				glm::ivec2 movement = mousePosition - lastClickPos;
				glm::vec2 movementAtSpeed = glm::vec2(movement);
				const float mouseSpeed = glm::length(movementAtSpeed);
				if (mouseSpeed > 0.00001f)
				{
					const float yawRotation = -movementAtSpeed.x * timeDeltaF + m_yawRotSpeed * timeDeltaF;
					m_yaw += m_mouseSensitivity * yawRotation;

					const float pitchRotation = -movementAtSpeed.y * timeDeltaF + m_pitchRotSpeed * timeDeltaF;
					m_pitch += m_mouseSensitivity * pitchRotation;
					m_pitch = glm::clamp(m_pitch, -c_maxPitchValue, c_maxPitchValue);
				}
			}
		}
		else
		{
			enabled = false;
		}
	}

	void Flycam::Update(const KeyboardState& kbState, double timeDelta)
	{
		R3_PROF_EVENT();

		const float timeDeltaF = (float)timeDelta;
		float forwardAmount = 0.0f;
		float strafeAmount = 0.0f;
		float speedMul = 1.0f;
		if (kbState.m_keyPressed[KEY_LSHIFT] || kbState.m_keyPressed[KEY_RSHIFT])
		{ 
			speedMul = 10.0f;
		}
		if (kbState.m_keyPressed[KEY_w])
		{
			forwardAmount = 1.0f;
		}
		if (kbState.m_keyPressed[KEY_s])
		{
			forwardAmount = -1.0f;
		}
		if (kbState.m_keyPressed[KEY_d])
		{
			strafeAmount = 1.0f;
		}
		if (kbState.m_keyPressed[KEY_a])
		{
			strafeAmount = -1.0f;
		}

		// move forward
		const float forward = speedMul * forwardAmount * m_forwardSpeed * timeDeltaF;
		m_position += m_lookDirection * forward;

		// strafe
		const float strafe = speedMul * strafeAmount * m_strafeSpeed * timeDeltaF;
		m_position += m_right * strafe;
	}

	void Flycam::Update(const ControllerRawState& controllerState, double timeDelta)
	{
		R3_PROF_EVENT();

		const float timeDeltaF = (float)timeDelta;
		const float xAxisRight = controllerState.m_rightStickAxes[0];
		const float yAxisRight = controllerState.m_rightStickAxes[1];
		const float xAxisLeft = controllerState.m_leftStickAxes[0];
		const float yAxisLeft = controllerState.m_leftStickAxes[1];

		float moveSpeedMulti = 1.0f + (controllerState.m_rightTrigger * m_speedMultiplier) + ((controllerState.m_leftTrigger * m_highSpeedMultiplier));

		const float yawRotation = -xAxisRight * m_yawRotSpeed * timeDeltaF;
		m_yaw += yawRotation * m_controllerSensitivity;

		const float pitchRotation = yAxisRight * m_pitchRotSpeed * timeDeltaF;
		m_pitch += pitchRotation * m_controllerSensitivity;
		m_pitch = glm::clamp(m_pitch, -c_maxPitchValue, c_maxPitchValue);

		// build direction from pitch, yaw
		glm::vec3 downZ(0.0f, 0.0f, -1.0f);
		m_lookDirection = glm::normalize(glm::rotateX(downZ, m_pitch));		
		m_lookDirection = glm::normalize(glm::rotateY(m_lookDirection, m_yaw));

		// build right + up vectors
		const glm::vec3 upY(0.0f, 1.0f, 0.0f);
		m_right = glm::cross(m_lookDirection, upY);

		// move forward
		const float forward = yAxisLeft * m_forwardSpeed  * moveSpeedMulti * timeDeltaF;
		m_position += m_lookDirection * forward;

		// strafe
		const float strafe = xAxisLeft * m_strafeSpeed * moveSpeedMulti * timeDeltaF;
		m_position += m_right * strafe;
	}
}