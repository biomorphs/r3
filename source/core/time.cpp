#include "time.h"
#include <sdl_timer.h>

namespace R3
{
	namespace Time
	{
		uint64_t HighPerformanceCounterTicks()
		{
			return SDL_GetPerformanceCounter();
		}

		uint64_t HighPerformanceCounterFrequency()
		{
			return SDL_GetPerformanceFrequency();
		}
	}
}