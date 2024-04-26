#include "engine_startup.h"
#include "frame_graph.h"
#include "systems.h"
#include "register_engine_components.h"
#include "systems/time_system.h"
#include "systems/event_system.h"
#include "systems/input_system.h"
#include "systems/imgui_system.h"
#include "systems/camera_system.h"
#include "systems/model_data_system.h"
#include "systems/static_mesh_system.h"
#include "systems/job_system.h"
#include "render/render_system.h"
#include "entities/systems/entity_system.h"
#include "core/platform.h"
#include "core/random.h"
#include "core/profiler.h"
#include "core/log.h"
#include <cassert>

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
		s.RegisterSystem<ImGuiSystem>();
		s.RegisterSystem<CameraSystem>();
		s.RegisterSystem<JobSystem>();
		s.RegisterSystem<Entities::EntitySystem>();
		s.RegisterSystem<ModelDataSystem>();
		s.RegisterSystem<StaticMeshSystem>();
	}

	// the default frame graph
	void BuildFrameGraph(FrameGraph& fg)
	{
		R3_PROF_EVENT();
		{
			auto& frameStart = fg.m_root.AddSequence("FrameStart");
			frameStart.AddFn("Time::FrameStart");
			frameStart.AddFn("Events::FrameStart");
			frameStart.AddFn("Input::FrameStart");
			frameStart.AddFn("ImGui::FrameStart");
		}
		{
			auto& fixedUpdate = fg.m_root.AddFixedUpdateSequence("FixedUpdate");
			fixedUpdate.AddFn("Cameras::FixedUpdate");
			fixedUpdate.AddFn("Time::FixedUpdateEnd");
		}
		{
			auto& varUpdate = fg.m_root.AddSequence("VariableUpdate");
			varUpdate.AddFn("Entities::RunGC");
		}
		{
			auto& guiUpdate = fg.m_root.AddSequence("ImGuiUpdate");
			guiUpdate.AddFn("Render::ShowGui");
			guiUpdate.AddFn("Entities::ShowGui");
			guiUpdate.AddFn("Cameras::ShowGui");
			guiUpdate.AddFn("Jobs::ShowGui");
			guiUpdate.AddFn("StaticMeshes::ShowGui");
			guiUpdate.AddFn("ModelData::ShowGui");
		}
		{
			auto& render = fg.m_root.AddSequence("Render");
			render.AddFn("Cameras::PreRenderUpdate");
			render.AddFn("Render::DrawFrame");
		}
	}

	// Engine entry point
	int Run(std::string_view fullCmdLine, RegisterSystemsFn systemCreation, BuildFrameGraphFn frameGraphBuildCb)
	{
		R3_PROF_EVENT();
		R3_PROF_THREAD("Main");

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
			LogError("Failed to initialise systems");
			return false;
		}

		// Register engine component types after systems init
		RegisterEngineComponents();

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

		// Shut down
		Systems::GetInstance().Shutdown();
		auto shutdownResult = Platform::Shutdown();
		assert(shutdownResult == Platform::ShutdownResult::ShutdownOK);
		return shutdownResult == Platform::ShutdownResult::ShutdownOK ? 0 : 1;
	}
}