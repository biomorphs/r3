#include "world_runner_window.h"
#include "engine/ui/basic_value_inspector.h"
#include "engine/ui/entity_list_widget.h"
#include "engine/ui/entity_inspector_widget.h"
#include "engine/ui/imgui_menubar_helper.h"
#include "engine/systems/mesh_renderer.h"
#include "entities/systems/entity_system.h"
#include "entities/world.h"
#include "core/profiler.h"
#include <format>
#include <filesystem>
#include <imgui.h>

namespace R3
{
	WorldRunnerWindow::WorldRunnerWindow(std::string worldIdentifier, std::string filePath)
		: m_worldID(worldIdentifier)
		, m_sceneFile(filePath)
	{
		m_allEntitiesWidget = std::make_unique<EntityListWidget>();
		m_allEntitiesWidget->m_options.m_canExpandEntities = false;
		m_allEntitiesWidget->m_options.m_showInternalIndex = false;
		m_allEntitiesWidget->m_options.m_onSelected = [this](const Entities::EntityHandle& e, bool append) {
			m_selectedEntities.push_back(e);
		};
		m_allEntitiesWidget->m_options.m_onDeselected = [this](const Entities::EntityHandle& e) {
			m_selectedEntities.erase(std::find(m_selectedEntities.begin(), m_selectedEntities.end(), e));
		};

		m_inspectEntityWidget = std::make_unique<EntityInspectorWidget>();
		m_inspectEntityWidget->m_onAddComponent = [this](const Entities::EntityHandle& h, std::string_view typeName) {
			GetWorld()->AddComponent(h, typeName);
		};
		m_inspectEntityWidget->m_onRemoveComponent = [this](const Entities::EntityHandle& h, std::string_view typeName) {
			GetWorld()->RemoveComponent(h, typeName);
		};
		m_inspectEntityWidget->m_onSetEntityName = [this](const Entities::EntityHandle& h, std::string_view oldName, std::string_view newName)
		{
			GetWorld()->SetEntityName(h, newName);
		};
	}

	WorldRunnerWindow::~WorldRunnerWindow()
	{
		auto entities = Systems::GetSystem<Entities::EntitySystem>();
		if (entities)
		{
			entities->DestroyWorld(m_worldID);
		}
		Systems::GetSystem<MeshRenderer>()->SetStaticsDirty();	// rebuild static scene
	}

	Entities::World* WorldRunnerWindow::GetWorld()
	{
		auto entities = Systems::GetSystem<Entities::EntitySystem>();
		return entities->GetWorld(m_worldID);
	}

	std::string_view WorldRunnerWindow::GetWindowTitle()
	{
		std::filesystem::path scenePath(m_sceneFile);
		static std::string title = std::format("Running {}", scenePath.filename().string());
		return title;
	}

	void WorldRunnerWindow::UpdateMainMenu()
	{
		R3_PROF_EVENT();
		auto scripts = Systems::GetSystem<LuaSystem>();

		auto& debugMenu = MenuBar::MainMenu().GetSubmenu("Debug");
		debugMenu.AddItem("Show Entity Inspector", [this]() {
			m_showInspector = true;
		});
		if (scripts->GetWorldScriptsActive())
		{
			debugMenu.AddItem("Disable Scripts", []() {
				auto scripts = Systems::GetSystem<LuaSystem>();
				scripts->SetWorldScriptsActive(false);
			});
		}
		else
		{
			debugMenu.AddItem("Enable Scripts", []() {
				auto scripts = Systems::GetSystem<LuaSystem>();
				scripts->SetWorldScriptsActive(true);
			});
		}
	}

	void WorldRunnerWindow::Update()
	{
		R3_PROF_EVENT();
		UpdateMainMenu();
		if (m_showInspector)
		{
			m_allEntitiesWidget->Update(*GetWorld(), m_selectedEntities, false);
			if (m_selectedEntities.size() > 0)
			{
				if (ImGui::Begin("Inspect Entities", &m_showInspector))
				{
					BasicValueInspector inspector;
					for (int i = 0; i < m_selectedEntities.size(); ++i)
					{
						m_inspectEntityWidget->Update(m_selectedEntities[i], *GetWorld(), inspector, true);
					}
				}
				ImGui::End();
			}
		}
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
		Systems::GetSystem<MeshRenderer>()->SetStaticsDirty();	// rebuild static scene
	}

	void WorldRunnerWindow::OnFocusLost()
	{
		R3_PROF_EVENT();
		auto scripts = Systems::GetSystem<LuaSystem>();
		scripts->SetWorldScriptsActive(false);
	}
}