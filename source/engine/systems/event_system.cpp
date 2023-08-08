#include "event_system.h"
#include "core/profiler.h"
#include <SDL_events.h>

namespace R3
{
	void EventSystem::RegisterTickFns()
	{
		Systems::GetInstance().RegisterTick("Events::FrameStart", [this]() {
			return OnFrameStart();
		});
	}

	void EventSystem::RegisterEventHandler(EventHandler h)
	{
		m_handlers.push_back(h);
	}

	bool EventSystem::OnFrameStart()
	{
		R3_PROF_EVENT("EventSystem::Tick");
		SDL_Event event;
		while (SDL_PollEvent(&event))
		{
			R3_PROF_EVENT("HandleEvent");
			for (auto& it : m_handlers)
			{
				it(&event);
			}
			if (event.type == SDL_QUIT)
			{
				return false;
			}
		}
		return true;
	}
}