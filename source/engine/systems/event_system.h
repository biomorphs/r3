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
	private:
		std::vector<EventHandler> m_handlers;
		bool OnFrameStart();
	};
}