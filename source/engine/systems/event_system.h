#pragma once
#include "engine/systems.h"

namespace R3
{
	// Handles SDE events and fires callbacks to registered handlers
	class EventSystem : public System
	{
	public:
		static std::string_view GetName() { return "Events"; }
		virtual void RegisterTickFns();

		using EventHandler = std::function<void(void*)>;
		void RegisterEventHandler(EventHandler);

		// if set, OnFrameStart will return false (and the app will close) if quit event is processed
		void SetCloseImmediate(bool b) { m_closeImmediately = b; }

	private:
		std::vector<EventHandler> m_handlers;
		bool OnFrameStart();
		bool m_closeImmediately = true;	
	};
}