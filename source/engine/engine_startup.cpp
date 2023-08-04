#include "engine_startup.h"
#include "systems.h"
#include "core/platform.h"
#include "core/random.h"
#include <cassert>
#include <fmt/format.h>

namespace R3
{
	void RegisterSystems(std::function<void()> systemCreation)
	{

	}

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

		// Register all engine and user systems
		RegisterSystems(systemCreation);

		// Initialise systems
		bool systemsInitialised = Systems::GetInstance().Initialise();
		if (!systemsInitialised)
		{
			fmt::print("Failed to initialise systems");
			return false;
		}

		// Build the frame graph
		// ...

		// Run the engine
		bool running = false;
		while (running)
		{

		}

		// Shut down systems
		Systems::GetInstance().Shutdown();

		// Shutdown
		auto shutdownResult = Platform::Shutdown();
		assert(shutdownResult == Platform::ShutdownResult::ShutdownOK);
		return shutdownResult == Platform::ShutdownResult::ShutdownOK ? 0 : 1;
	}
}