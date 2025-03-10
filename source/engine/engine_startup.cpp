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
#include "systems/texture_system.h"
#include "systems/static_mesh_system.h"
#include "systems/mesh_renderer.h"
#include "systems/lights_system.h"
#include "systems/job_system.h"
#include "systems/lua_system.h"
#include "systems/transform_system.h"
#include "systems/immediate_render_system.h"
#include "systems/frame_scheduler_system.h"
#include "systems/render_stats.h"
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
		s.RegisterSystem<FrameScheduler>();
		s.RegisterSystem<ImGuiSystem>();
		s.RegisterSystem<ImmediateRenderSystem>();
		s.RegisterSystem<CameraSystem>();
		s.RegisterSystem<JobSystem>();
		s.RegisterSystem<Entities::EntitySystem>();
		s.RegisterSystem<ModelDataSystem>();
		s.RegisterSystem<TextureSystem>();
		s.RegisterSystem<StaticMeshSystem>();
		s.RegisterSystem<MeshRenderer>();
		s.RegisterSystem<LightsSystem>();
		s.RegisterSystem<LuaSystem>();
		s.RegisterSystem<TransformSystem>();
		s.RegisterSystem<RenderStatsSystem>();
	}

	// the default frame graph
	void BuildFrameGraph(FrameGraph& fg)
	{
		R3_PROF_EVENT();
		{
			auto& frameStart = fg.m_root.AddSequence("FrameStart");
			frameStart.AddFn("Time::FrameStart");
			frameStart.AddFn("Events::FrameStart");
			frameStart.AddFn("Input::FrameStart");	// after events so any input updates are already sent
			frameStart.AddFn("ImGui::FrameStart");
		}
		auto& runAcquireAndUpdateAsync = fg.m_root.AddAsync("RunAcquireAndUpdate");	// first entry always runs on main thread
		{
			runAcquireAndUpdateAsync.AddFn("Render::AcquireSwapImage");	
			auto& updateSequence = runAcquireAndUpdateAsync.AddSequence("Upddate");
			auto& fixedUpdate = updateSequence.AddFixedUpdateSequence("FixedUpdate");
			{
				fixedUpdate.AddFn("Transforms::OnFixedUpdate");						// must run before any modifications
				fixedUpdate.AddFn("Cameras::FixedUpdate");
				fixedUpdate.AddFn("LuaSystem::RunFixedUpdateScripts");
			}
			auto& varUpdate = updateSequence.AddSequence("VariableUpdate");
			{
				varUpdate.AddFn("LuaSystem::RunVariableUpdateScripts");
				varUpdate.AddFn("Entities::RunGC");
				varUpdate.AddFn("LightsSystem::DrawLightBounds");
			}
			{
				auto& guiUpdate = updateSequence.AddSequence("ImGuiUpdate");
				guiUpdate.AddFn("Render::ShowGui");
				guiUpdate.AddFn("Entities::ShowGui");
				guiUpdate.AddFn("Cameras::ShowGui");
				guiUpdate.AddFn("Jobs::ShowGui");
				guiUpdate.AddFn("StaticMeshes::ShowGui");
				guiUpdate.AddFn("ModelData::ShowGui");
				guiUpdate.AddFn("MeshRenderer::ShowGui");
				guiUpdate.AddFn("Textures::ShowGui");
				guiUpdate.AddFn("LightsSystem::ShowGui");
				guiUpdate.AddFn("LuaSystem::ShowGui");
				guiUpdate.AddFn("FrameScheduler::ShowGui");
				guiUpdate.AddFn("RenderStats::ShowGui");
			}
			{
				auto& renderUpdate = updateSequence.AddSequence("RenderUpdate");
				renderUpdate.AddFn("Cameras::PreRenderUpdate");
				renderUpdate.AddFn("LightsSystem::PreRenderUpdate");
				{
					auto& renderASyncUpdate = renderUpdate.AddAsync("UpdateAsync");
					renderASyncUpdate.AddFn("MeshRenderer::CollectInstances");
					renderASyncUpdate.AddFn("LightsSystem::CollectPointLights");
					renderASyncUpdate.AddFn("LightsSystem::CollectSpotLights");
					renderASyncUpdate.AddFn("FrameScheduler::UpdateTonemapper");
				}
				renderUpdate.AddFn("LightsSystem::CollectShadowCasters");	// must happen after CollectSpotLights / CollectPointLights
				renderUpdate.AddFn("LightsSystem::CollectAllLightsData");
				renderUpdate.AddFn("FrameScheduler::BuildRenderGraph");		// must happen after LightsSystem::CollectAllLightsData
			}
			{
				auto& endUpdate = updateSequence.AddSequence("EndFrame");
				endUpdate.AddFn("Input::FrameEnd");
			}
		}
		auto& runRenderAndGC = fg.m_root.AddAsync("RenderAndGC");	// first entry always runs on main thread
		{
			auto& render = runRenderAndGC.AddSequence("Render");
			render.AddFn("Render::DrawFrame");
			runRenderAndGC.AddFn("LuaSystem::RunGC");
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