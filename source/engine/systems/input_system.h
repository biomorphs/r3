#pragma once

#include "engine/systems.h"
#include <array>

namespace R3
{
	enum Key {
		KEY_0,	KEY_1,	KEY_2,	KEY_3,	KEY_4,	KEY_5,	KEY_6,
		KEY_7,	KEY_8,	KEY_9,	KEY_a,	KEY_b,	KEY_c,	KEY_d,
		KEY_e,	KEY_f,	KEY_g,	KEY_h,	KEY_i,	KEY_j,	KEY_k,
		KEY_l,	KEY_m,	KEY_n,	KEY_o,	KEY_p,	KEY_q,	KEY_r,
		KEY_s,	KEY_t,	KEY_u,	KEY_v,	KEY_w,	KEY_x,	KEY_y,
		KEY_z,	KEY_LSHIFT,	KEY_RSHIFT,	KEY_SPACE,	KEY_LEFT, KEY_RIGHT,
		KEY_UP,	KEY_DOWN, KEY_LCTRL, KEY_RCTRL,	KEY_ESCAPE,	KEY_MAX
	};
	struct KeyboardState
	{
		std::array<bool, KEY_MAX> m_keyPressed = { false };
	};

	enum ControllerButtons
	{
		A = (1 << 0), B = (1 << 1), X = (1 << 2), Y = (1 << 3),
		Back = (1 << 4), Guide = (1 << 5), Start = (1 << 6),
		LeftStick = (1 << 7), RightStick = (1 << 8),
		LeftShoulder = (1 << 9), RightShoulder = (1 << 10),
		DPadUp = (1 << 11), DPadDown = (1 << 12), DPadLeft = (1 << 13), DPadRight = (1 << 14)
	};

	struct ControllerRawState
	{
		uint32_t m_buttonState;		// 1 bit/button, use Button enum to access
		float m_leftStickAxes[2];	// x, y, -1 to 1
		float m_rightStickAxes[2];	// x, y, -1 to 1
		float m_leftTrigger;		// 0 to 1
		float m_rightTrigger;		// 0 to 1
	};

	enum MouseButtons
	{
		LeftButton = (1 << 0),
		MiddleButton = (1 << 1),
		RightButton = (1 << 2)
	};

	struct MouseRawState
	{
		int32_t m_cursorX = 0;	// position relative to window/viewport in pixels
		int32_t m_cursorY = 0;
		uint32_t m_buttonState = 0;	// mask of buttons
		int32_t m_wheelScroll = 0;
	};

	class InputSystem : public System
	{
	public:
		static std::string_view GetName() { return "Input"; }
		virtual void RegisterTickFns();
		virtual bool Init();

		inline uint32_t ControllerCount() const { return (uint32_t)m_controllers.size(); }
		const ControllerRawState ControllerState(uint32_t padIndex) const;
		const MouseRawState& GetMouseState() const { return m_mouseState; }
		const KeyboardState& GetKeyboardState() const { return m_keysState;	}
		bool IsKeyDown(Key key);
		bool IsKeyDown(const char* keyStr);
		void SetKeyboardEnabled(bool enabled) { m_keysEnabled = enabled; }
		bool IsGuiCapturingInput();	// returns true if mouse or keyboard interacting with imgui

	private:
		bool OnFrameStart();
		void UpdateControllerState();
		void EnumerateControllers();
		float ApplyDeadZone(float input, float deadZone) const;
		void UpdateMouseState();
		void OnSystemEvent(void*);

		struct ControllerDesc
		{
			void* m_sdlController;
			ControllerRawState m_padState;
		};
		std::vector<ControllerDesc> m_controllers;
		float m_controllerAxisDeadZone = 0.5f;
		int32_t m_currentMouseScroll = 0;
		MouseRawState m_mouseState;
		KeyboardState m_keysState;
		bool m_keysEnabled = true;
	};
}