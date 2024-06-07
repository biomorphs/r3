#include "platform.h"
#include "core/profiler.h"
#include "core/file_io.h"
#include "core/log.h"
#include <cassert>
#include <SDL.h>

namespace R3
{
	namespace Platform
	{
		namespace Internal
		{
			std::string g_fullCmdLine;
		}

		SystemPowerState GetSystemPowerState()
		{
			SystemPowerState sps;
			SDL_PowerState sdlPs = SDL_GetPowerInfo(&sps.m_batterySecondsLeft, &sps.m_batteryPercentageRemaining);
			sps.m_isRunningOnBattery = (sdlPs == SDL_POWERSTATE_ON_BATTERY);
			return sps;
		}

		std::string_view GetCmdLine()
		{
			return Internal::g_fullCmdLine;
		}

		void ProcessCommandLine()
		{
#ifdef R3_USE_OPTICK
			if (GetCmdLine().find("-waitforprofiler") != std::string::npos)
			{
				LogInfo("Waiting for profiler connection...");
				while (!R3_PROF_IS_ACTIVE())
				{
					R3_PROF_FRAME("Main Thread");	// kick off the profiler with a fake frame
					R3_PROF_EVENT();
					SDL_Delay(10);
				}
			}
#endif
		}

		InitResult Initialise(std::string_view fullCmdLine)
		{
			Internal::g_fullCmdLine = fullCmdLine;
			R3::FileIO::InitialisePaths();
			R3::FileIO::AddBasePath("common");

			ProcessCommandLine();

			{
				R3_PROF_EVENT("SDL_Init");
				int sdlResult = SDL_Init(SDL_INIT_EVERYTHING);
				assert(sdlResult == 0);
				if (sdlResult != 0)
				{
					LogError("Failed to initialise SDL:\n\t{}", SDL_GetError());
					return InitResult::InitFailed;
				}
			}

			return InitResult::InitOK;
		}

		ShutdownResult Shutdown()
		{
			R3_PROF_EVENT();

			SDL_Quit();

			return ShutdownResult::ShutdownOK;
		}
	}
}
