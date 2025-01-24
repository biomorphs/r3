#include "world_editor_window.h"
#include "editor_utils.h"
#include "systems/editor_system.h"
#include "world_info_widget.h"
#include "editor_command_list.h"
#include "undo_redo_value_inspector.h"
#include "world_editor_tool.h"
#include "world_editor_select_entity_tool.h"
#include "world_editor_move_entities_tool.h"
#include "commands/set_value_cmd.h"
#include "commands/world_editor_save_cmd.h"
#include "commands/world_editor_select_entities_cmd.h"
#include "commands/world_editor_add_empty_entity_cmd.h"
#include "commands/world_editor_delete_entities_cmd.h"
#include "commands/world_editor_add_component_cmd.h"
#include "commands/world_editor_delete_component_cmd.h"
#include "commands/world_editor_clone_entities_cmd.h"
#include "commands/world_editor_add_entity_from_mesh_cmd.h"
#include "commands/world_editor_set_entity_parent_cmd.h"
#include "commands/world_editor_import_scene_cmd.h"
#include "engine/systems.h"
#include "engine/ui/entity_list_widget.h"
#include "engine/ui/entity_inspector_widget.h"
#include "engine/ui/imgui_menubar_helper.h"
#include "engine/ui/file_dialogs.h"
#include "engine/ui/reactive_value_inspector.h"
#include "engine/systems/lua_system.h"
#include "engine/systems/model_data_system.h"
#include "engine/systems/imgui_system.h"
#include "engine/systems/input_system.h"
#include "engine/systems/static_mesh_renderer.h"
#include "engine/components/transform.h"
#include "engine/components/static_mesh.h"
#include "engine/components/point_light.h"
#include "entities/systems/entity_system.h"
#include "entities/component_type_registry.h"
#include "render/render_system.h"
#include "core/file_io.h"
#include "imgui.h"
#include <format>

namespace R3
{
	constexpr size_t c_maxEntitiesToInspect = 64;	// imgui struggles with too many child windows

	WorldEditorWindow::WorldEditorWindow(std::string worldIdentifier, std::string filePath)
		: m_worldIdentifier(worldIdentifier)
		, m_filePath(filePath)
	{
		R3_PROF_EVENT();

		m_allEntitiesWidget = std::make_unique<EntityListWidget>();
		m_allEntitiesWidget->m_options.m_canExpandEntities = false;
		m_allEntitiesWidget->m_options.m_showInternalIndex = false;
		m_allEntitiesWidget->m_options.m_onSelected = [this](const Entities::EntityHandle& e, bool append) {
			auto selectCmd = std::make_unique<WorldEditorSelectEntitiesCommand>(this);
			selectCmd->m_appendToSelection = append;
			selectCmd->m_toSelect.push_back(e);
			m_cmds->Push(std::move(selectCmd));
		};
		m_allEntitiesWidget->m_options.m_onDeselected = [this](const Entities::EntityHandle& e) {
			auto selectCmd = std::make_unique<WorldEditorSelectEntitiesCommand>(this);
			selectCmd->m_toDeselect.push_back(e);
			m_cmds->Push(std::move(selectCmd));
		};

		m_inspectEntityWidget = std::make_unique<EntityInspectorWidget>();
		m_inspectEntityWidget->m_onAddComponent = [this](const Entities::EntityHandle& h, std::string_view typeName) {
			auto addComponent = std::make_unique<WorldEditorAddComponentCommand>(this);
			addComponent->m_targetEntities.push_back(h);
			addComponent->m_componentType = typeName;
			m_cmds->Push(std::move(addComponent));
		};
		m_inspectEntityWidget->m_onRemoveComponent = [this](const Entities::EntityHandle& h, std::string_view typeName) {
			auto deleteCmd = std::make_unique<WorldEditorDeleteComponentCommand>(this);
			deleteCmd->m_targetEntities.push_back(h);
			deleteCmd->m_componentType = typeName;
			m_cmds->Push(std::move(deleteCmd));
		};
		m_inspectEntityWidget->m_onSetEntityName = [this](const Entities::EntityHandle& h, std::string_view oldName, std::string_view newName)
		{
			auto setNameFn = [this, h](std::string n) {
				auto w = GetWorld();
				w->SetEntityName(h, n);
			};
			auto newCmd = std::make_unique<SetValueCommand<std::string>>("Set Entity Name", std::string(oldName), std::string(newName), setNameFn);
			m_cmds->Push(std::move(newCmd));
		};

		// Handle modification of anything owning a static mesh component
		m_inspectEntityWidget->m_onInspectEntity = [this](const Entities::EntityHandle& h, Entities::World& w) {
			m_isInspectingEntityWithStaticMesh = w.GetComponent<StaticMeshComponent>(h);
		};

		m_cmds = std::make_unique<EditorCommandList>();
		m_valueInspector = std::make_unique<ReactiveValueInspector>(std::make_unique<UndoRedoInspector>(*m_cmds));
		static_cast<ReactiveValueInspector*>(m_valueInspector.get())->SetOnValueChange([this]() {
			if (m_isInspectingEntityWithStaticMesh)
			{
				Systems::GetSystem<StaticMeshRenderer>()->SetStaticsDirty();
			}
		});

		CreateTools();

		auto scripts = Systems::GetSystem<LuaSystem>();
		scripts->SetWorldScriptsActive(false);	// don't run scripts in the editor
	}

	WorldEditorWindow::~WorldEditorWindow()
	{
		auto* entities = Systems::GetSystem<Entities::EntitySystem>();
		if (entities)
		{
			entities->DestroyWorld(m_worldIdentifier);
		}
	}

	void WorldEditorWindow::DeleteSelected()
	{
		R3_PROF_EVENT();
		auto world = GetWorld();
		for (int s = 0; s < m_selectedEntities.size(); ++s)
		{
			world->RemoveEntity(m_selectedEntities[s]);
		}
		m_selectedEntities.clear();
	}

	void WorldEditorWindow::UpdateMainContextMenu()
	{
		R3_PROF_EVENT();
		auto world = GetWorld();

		MenuBar contextMenu;
		auto& addEntityMenu = contextMenu.GetSubmenu("Add Entity");
		addEntityMenu.AddItem("Empty Entity", [this, world]() {
			m_cmds->Push(std::make_unique<WorldEditorAddEmptyEntityCommand>(this));
		});
		addEntityMenu.AddItem("From Mesh", [this, world]() {
			FileDialogFilter filters[] = {
				{ "Mesh Source File", "gltf,glb,fbx,obj" }
			};
			std::string meshPath = FileLoadDialog("", filters, std::size(filters));
			meshPath = FileIO::SanitisePath(meshPath);
			if (meshPath.length() > 0)
			{
				m_cmds->Push(std::make_unique<WorldEditorAddEntityFromMeshCommand>(this, meshPath));
			}
		});
		addEntityMenu.AddItem("Point Light", [this, world]() {
			auto addPointlight = [world](const Entities::EntityHandle& e) {
				world->AddComponent<PointLightComponent>(e);
				world->AddComponent<TransformComponent>(e);
			};
			auto addCmd = std::make_unique<WorldEditorAddEmptyEntityCommand>(this, "Point Light");
			addCmd->SetOnCreate(addPointlight);
			m_cmds->Push(std::move(addCmd));
		});
		contextMenu.AddItem("Import Scene", [this]() {
			FileDialogFilter filters[] = {
				{ "Scene File", "scn" }
			};
			std::string scnPath = FileLoadDialog("", filters, std::size(filters));
			scnPath = FileIO::SanitisePath(scnPath);
			if (scnPath.length() > 0)
			{
				m_cmds->Push(std::make_unique<WorldEditorImportSceneCmd>(this, scnPath));
			}
		});
		contextMenu.AddItem("Select all", [this]() {
			auto selectCmd = std::make_unique<WorldEditorSelectEntitiesCommand>(this);
			selectCmd->m_selectAll = true;
			m_cmds->Push(std::move(selectCmd));
		});
		if (m_selectedEntities.size() > 0)
		{
			contextMenu.AddItem("Clone selection", [this]() {
				m_cmds->Push(std::make_unique<WorldEditorCloneEntitiesCmd>(this));
			});
			contextMenu.AddItem("Deselect all", [this]() {
				auto selectCmd = std::make_unique<WorldEditorSelectEntitiesCommand>(this);
				selectCmd->m_deselectAll = true;
				m_cmds->Push(std::move(selectCmd));
			});
			std::string deleteStr = m_selectedEntities.size() == 1 ? "Delete selected entity" : "Delete selected entities";
			contextMenu.AddItem(deleteStr, [this]() {
				auto deleteCmd = std::make_unique<WorldEditorDeleteEntitiesCmd>(this);
				m_cmds->Push(std::move(deleteCmd));
			});
			contextMenu.AddItem("Set Parent Entity", [this]() {
				m_isSelectParentActive = true;
			});
		}
		contextMenu.DisplayContextMenu();
	}

	void WorldEditorWindow::DrawSideBarRight(Entities::World* w)
	{
		R3_PROF_EVENT();
		uint32_t sidebarFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove;
		auto windowFullExtents = Systems::GetSystem<RenderSystem>()->GetWindowExtents();
		ImGui::SetNextWindowPos({ windowFullExtents.x - m_sidebarRightWidth,ImGui::GetTextLineHeightWithSpacing() * 3 });
		ImGui::SetNextWindowSize(ImVec2(m_sidebarRightWidth, windowFullExtents.y - ImGui::GetTextLineHeightWithSpacing() * 3));
		if (ImGui::Begin("EntitiesSidebarRight", nullptr, sidebarFlags))
		{
			if (w)
			{
				auto startIndex = m_selectedEntities.size() - glm::min(m_selectedEntities.size(), c_maxEntitiesToInspect);
				for (size_t i = startIndex; i < m_selectedEntities.size(); ++i)
				{
					m_inspectEntityWidget->Update(m_selectedEntities[i], *w, *m_valueInspector, true);
				}
			}
		}
		float newWidth = ImGui::GetWindowWidth();
		ImGui::End();
		m_sidebarRightWidth = glm::max(newWidth, windowFullExtents.x * 0.05f);
		m_sidebarRightWidth = glm::min(m_sidebarRightWidth, windowFullExtents.x * 0.45f);
	}

	void WorldEditorWindow::DrawToolbar()
	{
		R3_PROF_EVENT();
		const float c_toolbarButtonSize = 24.0f;
		const auto buttonExtents = ImVec2(c_toolbarButtonSize, c_toolbarButtonSize);
		uint32_t flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground;
		auto windowFullExtents = Systems::GetSystem<RenderSystem>()->GetWindowExtents();
		ImGui::SetNextWindowPos({ 2 + m_sidebarLeftWidth,ImGui::GetTextLineHeightWithSpacing() * 3 });
		ImGui::SetNextWindowSize(ImVec2(c_toolbarButtonSize, m_tools.size() * c_toolbarButtonSize));
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
		ImGui::Begin("WorldEditToolbar", nullptr, flags);
		ImGui::PopStyleVar();
		{
			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
			ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.5, 0.5));
			for(int t=0;t<m_tools.size();++t)
			{
				ImGuiSelectableFlags selectFlags = 0;
				std::string btnText = std::format("{}##{}", m_tools[t]->GetToolbarButtonText().data(), t);
				bool isCurrentlyActive = m_activeTool == t;
				if (isCurrentlyActive)
				{
					Systems::GetSystem<ImGuiSystem>()->PushLargeBoldFont();
				}
				if (ImGui::Selectable(btnText.c_str(), isCurrentlyActive, selectFlags, buttonExtents))
				{
					ActivateTool(t);
				}
				if (isCurrentlyActive)
				{
					ImGui::PopFont();
				}
				if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
				{
					ImGui::SetTooltip(m_tools[t]->GetTooltipText().data());
				}
			}
			ImGui::PopStyleVar();
			ImGui::PopStyleVar();
		}
		ImGui::End();

		auto input = Systems::GetSystem<InputSystem>();
		for (int t = 0; t < m_tools.size() && t < 9; ++t)
		{
			Key testKey = static_cast<Key>(t + static_cast<int>(KEY_1));
			if (input->WasKeyReleased(testKey))
			{
				ActivateTool(t);
			}
		}
	}

	void WorldEditorWindow::CreateTools()
	{
		m_tools.push_back(std::make_unique<WorldEditorSelectEntityTool>(this));
		m_tools.push_back(std::make_unique<WorldEditorMoveEntitiesTool>(this));
	}

	void WorldEditorWindow::ActivateTool(int toolIndex)
	{
		assert(toolIndex < m_tools.size());
		if (toolIndex >= m_tools.size())
		{
			return;
		}

		if (m_activeTool != -1)
		{
			if (!m_tools[m_activeTool]->OnDeactivated(*GetWorld(), *m_cmds))	// can't deactivate yet
			{
				return;
			}
		}
		if (toolIndex != -1 && toolIndex != m_activeTool)
		{
			if (!m_tools[toolIndex]->OnActivated(*GetWorld(), *m_cmds))
			{
				return;
			}
		}
		if (toolIndex == m_activeTool)
		{
			m_activeTool = -1;
		}
		else
		{
			m_activeTool = toolIndex;
		}
	}

	void WorldEditorWindow::UpdateMainMenu()
	{
		R3_PROF_EVENT();
		auto& fileMenu = MenuBar::MainMenu().GetSubmenu("File");
		fileMenu.AddItemAfter("Edit World", "Save World", [this]() {
			m_cmds->Push(std::make_unique<WorldEditorSaveCmd>(this, m_filePath));
			});
		fileMenu.AddItemAfter("Save World", "Save World As", [this]() {
			m_cmds->Push(std::make_unique<WorldEditorSaveCmd>(this, ""));
			});
		auto& editMenu = MenuBar::MainMenu().GetSubmenu("Edit");
		editMenu.AddItem("Undo", [this]() {
			m_cmds->Undo();
			}, "", m_cmds->CanUndo());
		editMenu.AddItem("Redo", [this]() {
			m_cmds->Redo();
			}, "", m_cmds->CanRedo());
		auto& debugMenu = MenuBar::MainMenu().GetSubmenu("Debug");
		debugMenu.AddItem("Editor Command History", [this]() {
			m_showCommandsWindow = true;
		});
	}

	void WorldEditorWindow::DrawSideBarLeft(Entities::World* w)
	{
		R3_PROF_EVENT();
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
				m_allEntitiesWidget->Update(*w, m_selectedEntities, true);
			}
		}
		float newWidth = ImGui::GetWindowWidth();
		ImGui::End();
		m_sidebarLeftWidth = glm::max(newWidth, windowFullExtents.x * 0.05f);
		m_sidebarLeftWidth = glm::min(m_sidebarLeftWidth, windowFullExtents.x * 0.45f);
	}

	std::string_view WorldEditorWindow::GetWindowTitle()
	{
		std::string worldName(GetWorld()->GetName());
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

	void WorldEditorWindow::UpdateSelectParentWindow()
	{
		R3_PROF_EVENT();

		uint32_t popupFlags = ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings;
		std::string windowName("Select Parent Entity");
		ImGui::OpenPopup(windowName.c_str());
		if (ImGui::BeginPopupModal(windowName.c_str(), nullptr, popupFlags))
		{
			if (ImGui::Button("None"))
			{
				m_cmds->Push(std::make_unique<WorldEditorSetEntityParentCmd>(this, Entities::EntityHandle()));
				m_isSelectParentActive = false;
			}
			auto w = GetWorld();
			std::vector<Entities::EntityHandle> allEntities = w->GetActiveEntities();
			for (const auto& e : allEntities)
			{
				std::string txt = std::format("{}##{}", w->GetEntityDisplayName(e), e.GetID());
				if (ImGui::Button(txt.c_str()))
				{
					m_cmds->Push(std::make_unique<WorldEditorSetEntityParentCmd>(this, e));
					m_isSelectParentActive = false;
				}
			}
			ImGui::EndPopup();
		}
	}

	void WorldEditorWindow::DrawSelected()
	{
		R3_PROF_EVENT();
		auto theWorld = GetWorld();
		auto modelDataSys = Systems::GetSystem<ModelDataSystem>();
		for (auto& theEntity : m_selectedEntities)
		{
			DrawEntityBounds(*theWorld, theEntity, { 1,1,0,1 });
			DrawParentLines(*theWorld, theEntity, { 0.1f,0.1f,0.3f,1.0f });
		}
	}

	void WorldEditorWindow::Update()
	{
		R3_PROF_EVENT();
		auto theWorld = GetWorld();
		UpdateMainMenu();
		DrawSideBarLeft(theWorld);
		DrawSideBarRight(theWorld);
		DrawToolbar();
		UpdateMainContextMenu();
		if (m_showCommandsWindow)
		{
			m_showCommandsWindow = m_cmds->ShowWidget();
		}
		DrawSelected();
		if (m_isSelectParentActive)
		{
			UpdateSelectParentWindow();
		}
		if (m_activeTool != -1 && m_activeTool < m_tools.size())
		{
			m_tools[m_activeTool]->Update(*GetWorld(), *m_cmds);
		}

		auto input = Systems::GetSystem<InputSystem>();
		auto editor = Systems::GetSystem<EditorSystem>();
		if (input->WasKeyReleased(Key::KEY_F5) && m_filePath.size() != 0)
		{
			editor->RunWorld(m_filePath);
		}

		m_cmds->RunNext();
	}

	EditorWindow::CloseStatus WorldEditorWindow::PrepareToClose()
	{
		R3_PROF_EVENT();
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
		auto entities = Systems::GetSystem<Entities::EntitySystem>();
		auto scripts = Systems::GetSystem<LuaSystem>();
		entities->SetActiveWorld(m_worldIdentifier);
		scripts->SetWorldScriptsActive(false);	// don't run scripts in the editor
		Systems::GetSystem<StaticMeshRenderer>()->SetStaticsDirty();	// rebuild static scene
	}

	void WorldEditorWindow::PushCommand(std::unique_ptr<EditorCommand>&& cmd)
	{
		m_cmds->Push(std::move(cmd));
	}

	bool WorldEditorWindow::SaveWorld(std::string_view path)
	{
		R3_PROF_EVENT();
		Entities::World* thisWorld = GetWorld();
		if (thisWorld && thisWorld->Save(path))
		{
			m_filePath = path;
			return true;
		}	
		return false;
	}

	Entities::World* WorldEditorWindow::GetWorld() const
	{
		auto* entities = Systems::GetSystem<Entities::EntitySystem>();
		if (entities)
		{
			return entities->GetWorld(m_worldIdentifier);
		}
		return nullptr;
	}

	void WorldEditorWindow::SelectEntities(const std::vector<Entities::EntityHandle>& h)
	{
		R3_PROF_EVENT();

		size_t insertIndex = m_selectedEntities.size();

		// compare the list against the previously selected entities
		for (const auto& toAdd : h)
		{
			auto foundIt = std::find(m_selectedEntities.begin(), m_selectedEntities.begin() + insertIndex, toAdd);
			if (foundIt == m_selectedEntities.begin() + insertIndex)
			{
				m_selectedEntities.push_back(toAdd);
			}
		}
	}

	void WorldEditorWindow::SelectEntity(const Entities::EntityHandle& h)
	{
		R3_PROF_EVENT();
		auto foundIt = std::find(m_selectedEntities.begin(), m_selectedEntities.end(), h);
		if (foundIt == m_selectedEntities.end())
		{
			m_selectedEntities.push_back(h);
		}
	}

	void WorldEditorWindow::DeselectEntity(const Entities::EntityHandle& h)
	{
		R3_PROF_EVENT();
		auto foundIt = std::find(m_selectedEntities.begin(), m_selectedEntities.end(), h);
		if (foundIt != m_selectedEntities.end())
		{
			m_selectedEntities.erase(foundIt);
		}
	}

	void WorldEditorWindow::DeselectAll()
	{
		R3_PROF_EVENT();
		m_selectedEntities.clear();
	}

	void WorldEditorWindow::SelectAll()
	{
		R3_PROF_EVENT();
		Entities::World* thisWorld = GetWorld();
		if (thisWorld)
		{
			m_selectedEntities = thisWorld->GetActiveEntities();
		}
	}
}