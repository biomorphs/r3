#include "engine_startup.h"
#include "core/platform.h"
#include "core/random.h"
#include <cassert>

namespace R3
{
	// Engine entry point
	int Run(std::string_view fullCmdLine, std::function<void()> systemCreation, std::function<void(FrameGraph&)> frameGraphBuildCb)
	{
		auto result = R3::Platform::Initialise(fullCmdLine);
		assert(result == R3::Platform::InitOK);
		if (result == R3::Platform::InitFailed)
		{
			return 1;
		}

		// Init random number generator
		R3::Random::ResetGlobalSeed();

		// Run the engine

		// Shutdown
		auto shutdownResult = Platform::Shutdown();
		assert(shutdownResult == Platform::ShutdownResult::ShutdownOK);
		return shutdownResult == Platform::ShutdownResult::ShutdownOK ? 0 : 1;
	}
}