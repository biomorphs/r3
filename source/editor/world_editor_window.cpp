#include "world_editor_window.h"
#include "world_info_widget.h"
#include "editor_command_list.h"
#include "editor/commands/world_editor_save_cmd.h"
#include "engine/systems.h"
#include "engine/basic_value_inspector.h"
#include "engine/entity_list_widget.h"
#include "engine/entity_inspector_widget.h"
#include "engine/imgui_menubar_helper.h"
#include "entities/systems/entity_system.h"
#include "render/render_system.h"
#include "imgui.h"
#include <format>

namespace R3
{
	WorldEditorWindow::WorldEditorWindow(std::string worldIdentifier, std::string filePath)
		: m_worldIdentifier(worldIdentifier)
		, m_filePath(filePath)
	{
		m_allEntitiesWidget = std::make_unique<EntityListWidget>();
		m_allEntitiesWidget->m_options.m_canExpandEntities = false;
		m_allEntitiesWidget->m_options.m_showInternalIndex = false;
		m_allEntitiesWidget->m_options.m_onSelected = [this](const Entities::EntityHandle& e) {
			m_selectedEntity = e;
		};
		m_inspectEntityWidget = std::make_unique<EntityInspectorWidget>();
		m_valueInspector = std::make_unique<BasicValueInspector>();
		m_cmds = std::make_unique<EditorCommandList>();
	}

	WorldEditorWindow::~WorldEditorWindow()
	{
		auto* entities = Systems::GetSystem<Entities::EntitySystem>();
		if (entities)
		{
			entities->DestroyWorld(m_worldIdentifier);
		}
	}

	void WorldEditorWindow::DrawSideBarRight(Entities::World* w)
	{
		uint32_t sidebarFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove;
		auto windowFullExtents = Systems::GetSystem<RenderSystem>()->GetWindowExtents();
		ImGui::SetNextWindowPos({ windowFullExtents.x - m_sidebarRightWidth,ImGui::GetTextLineHeightWithSpacing() * 3 });
		ImGui::SetNextWindowSize(ImVec2(m_sidebarRightWidth, windowFullExtents.y - ImGui::GetTextLineHeightWithSpacing() * 3));
		if (ImGui::Begin("EntitiesSidebarRight", nullptr, sidebarFlags))
		{
			if (w)
			{
				m_inspectEntityWidget->Update(m_selectedEntity, *w, *m_valueInspector, true);
			}
		}
		float newWidth = ImGui::GetWindowWidth();
		ImGui::End();
		m_sidebarRightWidth = glm::max(newWidth, windowFullExtents.x * 0.05f);
		m_sidebarRightWidth = glm::min(m_sidebarRightWidth, windowFullExtents.x * 0.45f);
	}

	void WorldEditorWindow::UpdateMainMenu()
	{
		auto& fileMenu = MenuBar::MainMenu().GetSubmenu("File");
		fileMenu.AddItemAfter("Open World", "Save World", [this]() {
			m_cmds->Push(std::make_unique<WorldEditorSaveCmd>(this, m_filePath));
		});
		fileMenu.AddItemAfter("Save World", "Save World As", [this]() {
			m_cmds->Push(std::make_unique<WorldEditorSaveCmd>(this, ""));
		});
		auto& settingsMenu = MenuBar::MainMenu().GetSubmenu("Settings");
		settingsMenu.AddItem("Open Command List Window", [this]() {
			m_showCommandsWindow = true;
		});
	}

	void WorldEditorWindow::DrawSideBarLeft(Entities::World* w)
	{
		uint32_t sidebarFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove;
		auto windowFullExtents = Systems::GetSystem<RenderSystem>()->GetWindowExtents();
		ImGui::SetNextWindowPos({ 0,ImGui::GetTextLineHeightWithSpacing() * 3 });
		ImGui::SetNextWindowSize(ImVec2(m_sidebarLeftWidth, windowFullExtents.y - ImGui::GetTextLineHeightWithSpacing() * 3));
		if (ImGui::Begin("EntitiesSidebarLeft", nullptr, sidebarFlags))
		{
			if (w)
			{
				WorldInfoWidget wi;
				wi.Update(*w, true);
				m_allEntitiesWidget->Update(*w, true);
			}
		}
		float newWidth = ImGui::GetWindowWidth();
		ImGui::End();
		m_sidebarLeftWidth = glm::max(newWidth, windowFullExtents.x * 0.05f);
		m_sidebarLeftWidth = glm::min(m_sidebarLeftWidth, windowFullExtents.x * 0.45f);
	}

	std::string_view WorldEditorWindow::GetWindowTitle()
	{
		auto entities = Systems::GetInstance().GetSystem<Entities::EntitySystem>();
		std::string worldName(entities->GetWorld(m_worldIdentifier)->GetName());

		if (m_filePath.empty())
		{
			m_titleString = std::format("World '{}'", worldName);
		}
		else
		{
			m_titleString = std::format("World '{}' ({})", worldName, m_filePath.c_str());
		}
		
		return m_titleString;
	}

	void WorldEditorWindow::Update()
	{
		R3_PROF_EVENT();
		UpdateMainMenu();
		auto* entities = Systems::GetSystem<Entities::EntitySystem>();
		if (entities)
		{
			Entities::World* thisWorld = entities->GetWorld(m_worldIdentifier);
			DrawSideBarLeft(thisWorld);
			DrawSideBarRight(thisWorld);
		}
		if (m_showCommandsWindow)
		{
			m_showCommandsWindow = m_cmds->ShowWidget();
		}
		m_cmds->RunNext();
	}

	EditorWindow::CloseStatus WorldEditorWindow::PrepareToClose()
	{
		bool shouldCloseWindow = false;
		bool shouldCancelRequest = false;
		ImGui::OpenPopup("Close window?");
		if (ImGui::BeginPopupModal("Close window?"))
		{
			ImGui::Text("Close current window?");
			if (ImGui::Button("Yes"))
			{
				shouldCloseWindow = true;
				ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine();
			if (ImGui::Button("No"))
			{
				shouldCancelRequest = true;
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}
		if (shouldCloseWindow)
		{
			return EditorWindow::CloseStatus::ReadyToClose;
		}
		else if (shouldCancelRequest)
		{
			return EditorWindow::CloseStatus::Cancel;
		}
		else
		{
			return EditorWindow::CloseStatus::NotReady;
		}
	}

	void WorldEditorWindow::OnFocusGained()
	{
		auto* entities = Systems::GetSystem<Entities::EntitySystem>();
		LogInfo("Setting active world '{}'", m_worldIdentifier);
		entities->SetActiveWorld(m_worldIdentifier);
	}

	bool WorldEditorWindow::SaveWorld(std::string_view path)
	{
		auto* entities = Systems::GetSystem<Entities::EntitySystem>();
		if (entities)
		{
			Entities::World* thisWorld = entities->GetWorld(m_worldIdentifier);
			if (thisWorld)
			{
				if (thisWorld->Save(path))
				{
					m_filePath = path;
					return true;
				}
			}
		}		
		return false;
	}
}