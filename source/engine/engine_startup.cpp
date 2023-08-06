#include "engine_startup.h"
#include "frame_graph.h"
#include "systems.h"
#include "systems/time_system.h"
#include "systems/event_system.h"
#include "systems/input_system.h"
#include "render/render_system.h"
#include "core/platform.h"
#include "core/random.h"
#include "core/profiler.h"
#include <cassert>
#include <fmt/format.h>

namespace R3
{
	// All default engine systems go here
	void RegisterSystems()
	{
		R3_PROF_EVENT();
		auto& s = Systems::GetInstance();
		s.RegisterSystem<TimeSystem>();
		s.RegisterSystem<EventSystem>();
		s.RegisterSystem<InputSystem>();
		s.RegisterSystem<RenderSystem>();
	}

	// the default frame graph of the engine describes the entire frame structure
	void BuildFrameGraph(FrameGraph& fg)
	{
		R3_PROF_EVENT();
		{
			auto& frameStart = fg.m_root.AddSequence("FrameStart");
			frameStart.AddFn("Time::FrameStart");
			frameStart.AddFn("Events::FrameStart");
			frameStart.AddFn("Input::FrameStart");
		}
		{
			auto& fixedUpdate = fg.m_root.AddFixedUpdateSequence("FixedUpdate");
			fixedUpdate.AddFn("Time::FixedUpdateEnd");
		}
		{
			auto& varUpdate = fg.m_root.AddSequence("VariableUpdate");
		}
		{
			auto& render = fg.m_root.AddSequence("Render");
			render.AddFn("Render::DrawFrame");
		}
	}

	// Engine entry point
	int Run(std::string_view fullCmdLine, RegisterSystemsFn systemCreation, BuildFrameGraphFn frameGraphBuildCb)
	{
		R3_PROF_EVENT();

		auto result = R3::Platform::Initialise(fullCmdLine);
		assert(result == R3::Platform::InitOK);
		if (result == R3::Platform::InitFailed)
		{
			return 1;
		}

		// Init random number generator
		R3::Random::ResetGlobalSeed();

		// Register all engine and user systems
		RegisterSystems();
		if (systemCreation)
		{
			systemCreation();
		}

		// Initialise systems (also registers tick fns)
		bool systemsInitialised = Systems::GetInstance().Initialise();
		if (!systemsInitialised)
		{
			fmt::print("Failed to initialise systems");
			return false;
		}

		// Build the frame graph
		FrameGraph runFrame;
		BuildFrameGraph(runFrame);
		if (frameGraphBuildCb)
		{
			frameGraphBuildCb(runFrame);
		}

		// Run the engine
		bool running = true;
		while (running)
		{
			R3_PROF_FRAME("Main Thread");
			running = runFrame.m_root.Run();
		}

		// Shut down systems
		Systems::GetInstance().Shutdown();

		// Shutdown
		auto shutdownResult = Platform::Shutdown();
		assert(shutdownResult == Platform::ShutdownResult::ShutdownOK);
		return shutdownResult == Platform::ShutdownResult::ShutdownOK ? 0 : 1;
	}
}