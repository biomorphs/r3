#include "world_runner_window.h"
#include "entities/systems/entity_system.h"
#include "core/profiler.h"
#include <format>
#include <filesystem>
namespace R3
{
	WorldRunnerWindow::WorldRunnerWindow(std::string worldIdentifier, std::string filePath)
		: m_worldID(worldIdentifier)
		, m_sceneFile(filePath)
	{
	}

	WorldRunnerWindow::~WorldRunnerWindow()
	{
		auto entities = Systems::GetSystem<Entities::EntitySystem>();
		if (entities)
		{
			entities->DestroyWorld(m_worldID);
		}
	}

	std::string_view WorldRunnerWindow::GetWindowTitle()
	{
		std::filesystem::path scenePath(m_sceneFile);
		static std::string title = std::format("Running {}", scenePath.filename().string());
		return title;
	}

	void WorldRunnerWindow::Update()
	{
		R3_PROF_EVENT();
	}

	EditorWindow::CloseStatus WorldRunnerWindow::PrepareToClose()
	{
		R3_PROF_EVENT();
		return CloseStatus::ReadyToClose;
	}

	void WorldRunnerWindow::OnFocusGained()
	{
		R3_PROF_EVENT();
		auto* entities = Systems::GetSystem<Entities::EntitySystem>();
		entities->SetActiveWorld(m_worldID);
		auto scripts = Systems::GetSystem<LuaSystem>();
		scripts->SetWorldScriptsActive(true);
	}

	void WorldRunnerWindow::OnFocusLost()
	{
		R3_PROF_EVENT();
		auto scripts = Systems::GetSystem<LuaSystem>();
		scripts->SetWorldScriptsActive(false);
	}
}