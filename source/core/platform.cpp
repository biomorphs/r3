#include "platform.h"
#include "core/profiler.h"
#include "core/file_io.h"
#include <cassert>
#include <SDL.h>
#include <fmt/format.h>

namespace R3
{
	namespace Platform
	{
		namespace Internal
		{
			std::string g_fullCmdLine;
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
				fmt::print("Waiting for profiler connection...\n");
				while (!R3_PROF_IS_ACTIVE())
				{
					R3_PROF_FRAME("Main Thread");	// kick off the profiler with a fake frame
					R3_PROF_EVENT();
					_sleep(20);
				}
			}
#endif
		}

		InitResult Initialise(std::string_view fullCmdLine)
		{
			Internal::g_fullCmdLine = fullCmdLine;
			R3::FileIO::InitialisePaths();

			ProcessCommandLine();

			fmt::print("Initialising SDL\n");
			int sdlResult = SDL_Init(SDL_INIT_EVERYTHING);
			assert(sdlResult == 0);
			if (sdlResult != 0)
			{
				fmt::print("Failed to initialise SDL:\r\n\t{}\n", SDL_GetError());
				return InitResult::InitFailed;
			}



			return InitResult::InitOK;
		}

		ShutdownResult Shutdown()
		{
			fmt::print("Shutting down SDL\n");

			SDL_Quit();

			return ShutdownResult::ShutdownOK;
		}
	}
}
