#include "editor_system.h"
#include "editor/world_editor_window.h"
#include "engine/imgui_menubar_helper.h"
#include "engine/systems/event_system.h"
#include "engine/file_dialogs.h"
#include "entities/systems/entity_system.h"
#include "render/render_system.h"
#include "core/profiler.h"
#include "imgui.h"
#include <format>
#include <SDL_events.h>

namespace R3
{
	void EditorSystem::RegisterTickFns()
	{
		R3_PROF_EVENT();
		RegisterTick("EditorSystem::ShowGui", [this] {
			return ShowGui();
		});
	}

	bool EditorSystem::Init()
	{
		R3_PROF_EVENT();

		// the editor will handle shutdown events
		auto events = GetSystem<EventSystem>();
		if (events)
		{	
			events->SetCloseImmediate(false);
			events->RegisterEventHandler([this](void* theEvent) {
				SDL_Event* actualEvent = static_cast<SDL_Event*>(theEvent);
				if (actualEvent->type == SDL_QUIT)
				{
					CloseAllWindows();
					m_quitRequested = true;
				}
			});
		}

		OnNewWorld();

		return true;
	}

	void EditorSystem::Shutdown()
	{
		R3_PROF_EVENT();
	}

	void EditorSystem::CloseWindow(EditorWindow* window)
	{
		auto found = std::find_if(m_allWindows.begin(), m_allWindows.end(), [window](const std::unique_ptr<EditorWindow>& w) {
			return w.get() == window;
		});
		assert(found != m_allWindows.end());
		if (found != m_allWindows.end())
		{
			m_windowsToClose.emplace(window);
		}
	}

	void EditorSystem::OnNewWorld()
	{
		R3_PROF_EVENT();
		auto entities = GetSystem<Entities::EntitySystem>();
		int newWorldNameId = m_worldInternalNameCounter++;
		std::string worldInternalName = std::format("EditorWorld_{}", newWorldNameId);
		Entities::World* newWorld = entities->CreateWorld(worldInternalName);
		if (newWorld)
		{
			newWorld->SetName("New World");
			m_allWindows.push_back(std::make_unique<WorldEditorWindow>(worldInternalName));
		}
	}

	void EditorSystem::OnOpenWorld()
	{
		R3_PROF_EVENT();
		std::string fileToOpen = FileLoadDialog("", "scn");
		if (!fileToOpen.empty())
		{
			auto entities = GetSystem<Entities::EntitySystem>();
			int newWorldNameId = m_worldInternalNameCounter++;
			std::string worldInternalName = std::format("EditorWorld_{}", newWorldNameId);
			Entities::World* newWorld = entities->CreateWorld(worldInternalName);
			if (newWorld && newWorld->Load(fileToOpen))
			{
				m_allWindows.push_back(std::make_unique<WorldEditorWindow>(worldInternalName, fileToOpen));
			}
			else
			{
				LogError("Failed to load world file '{}'", fileToOpen);
				entities->DestroyWorld(worldInternalName);
			}
		}
	}

	void EditorSystem::ShowMainMenu()
	{
		R3_PROF_EVENT();
		auto& fileMenu = MenuBar::MainMenu().GetSubmenu("File");
		fileMenu.AddItem("New World", [this]() {
			OnNewWorld();
		});
		fileMenu.AddItem("Open World", [this]() {
			OnOpenWorld();
		});
		fileMenu.AddItem("Exit", [this]() {
			CloseAllWindows();
			m_quitRequested = true;
		});
	}

	void EditorSystem::CloseAllWindows()
	{
		R3_PROF_EVENT();
		for (int i = 0; i < m_allWindows.size(); ++i)
		{
			CloseWindow(m_allWindows[i].get());
		}
	}

	void EditorSystem::ShowWindowTabs()
	{
		R3_PROF_EVENT();
		uint32_t windowFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar;
		auto windowFullExtents = GetSystem<RenderSystem>()->GetWindowExtents();
		ImGui::SetNextWindowPos({ 0,ImGui::GetTextLineHeightWithSpacing() + 1 });
		ImGui::SetNextWindowSize(ImVec2(windowFullExtents.x, 0));
		if (ImGui::Begin("##EditorWindowTabs", nullptr, windowFlags))
		{
			if (ImGui::BeginTabBar("Windows", ImGuiTabBarFlags_Reorderable | ImGuiTabBarFlags_FittingPolicyScroll | ImGuiTabBarFlags_AutoSelectNewTabs))
			{
				for (int window = 0; window < m_allWindows.size(); window++)
				{
					std::string uniqueId = std::format("{}##{}", m_allWindows[window]->GetWindowTitle(), window);
					bool tabIsOpen = true;	// so we can detect if the tab is closed
					if (ImGui::BeginTabItem(uniqueId.c_str(), &tabIsOpen))
					{
						m_selectedWindowTab = window;
						ImGui::EndTabItem();
					}
					if (!tabIsOpen)
					{
						CloseWindow(m_allWindows[window].get());
					}
				}
				ImGui::EndTabBar();
			}
		}
		ImGui::End();
	}

	void EditorSystem::ProcessClosingWindows()
	{
		R3_PROF_EVENT();
		std::vector<EditorWindow*> destroyedWindows;
		std::vector<EditorWindow*> cancelledWindows;
		for (auto& toClose : m_windowsToClose)
		{
			EditorWindow::CloseStatus closeResult = toClose->PrepareToClose();
			if (closeResult == EditorWindow::CloseStatus::ReadyToClose)
			{
				destroyedWindows.push_back(toClose);
			}
			else if (closeResult == EditorWindow::CloseStatus::Cancel)
			{
				cancelledWindows.push_back(toClose);
				m_quitRequested = false;	// a window cancelled closing, don't kill the editor yet
			}
			else
			{
				// switch to this window and stop for this frame
				auto found = std::find_if(m_allWindows.begin(), m_allWindows.end(), [&toClose](const std::unique_ptr<EditorWindow>& w) {
					return w.get() == toClose;
				});
				if (found != m_allWindows.end())
				{
					m_selectedWindowTab = static_cast<int>(std::distance(m_allWindows.begin(), found));
				}
				break;
			}
		}
		for (int i = 0; i < cancelledWindows.size(); ++i)
		{
			m_windowsToClose.erase(cancelledWindows[i]);
		}
		for (int i = 0; i < destroyedWindows.size(); ++i)
		{
			m_windowsToClose.erase(destroyedWindows[i]);
			for (int w = 0; w < m_allWindows.size(); ++w)
			{
				if (m_allWindows[w].get() == destroyedWindows[i])
				{
					m_allWindows.erase(m_allWindows.begin() + w);
					m_selectedWindowTab = -1;
					m_activeWindowIndex = -1;	// ensures focus gained/lost called correctly
					break;
				}
			}
		}
	}

	bool EditorSystem::ShowGui()
	{
		R3_PROF_EVENT();

		ApplyStyle();
		ShowMainMenu();
		ShowWindowTabs();
		
		if (m_selectedWindowTab != -1 && m_selectedWindowTab != m_activeWindowIndex)	// window selection changed?
		{
			if (m_activeWindowIndex != -1 && m_activeWindowIndex < m_allWindows.size())
			{
				m_allWindows[m_activeWindowIndex]->OnFocusLost();
			}
			m_allWindows[m_selectedWindowTab]->OnFocusGained();
			m_activeWindowIndex = m_selectedWindowTab;
		}
		if (m_selectedWindowTab >= 0 && m_selectedWindowTab < m_allWindows.size())
		{
			m_allWindows[m_selectedWindowTab]->Update();
		}

		ProcessClosingWindows();

		if (m_quitRequested)
		{
			return m_allWindows.size() != 0;
		}
		else
		{
			return true;
		}
	}

	void EditorSystem::ApplyStyle()
	{
		R3_PROF_EVENT();
		// generated via imgui demo style editor
		// its a bit gray
		ImVec4* colors = ImGui::GetStyle().Colors;
		colors[ImGuiCol_FrameBg] = ImVec4(0.24f, 0.24f, 0.24f, 0.54f);
		colors[ImGuiCol_FrameBgHovered] = ImVec4(0.54f, 0.54f, 0.54f, 0.40f);
		colors[ImGuiCol_FrameBgActive] = ImVec4(0.42f, 0.42f, 0.42f, 1.00f);
		colors[ImGuiCol_TitleBgActive] = ImVec4(0.06f, 0.06f, 0.06f, 1.00f);
		colors[ImGuiCol_CheckMark] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
		colors[ImGuiCol_SliderGrab] = ImVec4(0.52f, 0.52f, 0.52f, 1.00f);
		colors[ImGuiCol_SliderGrabActive] = ImVec4(0.73f, 0.73f, 0.73f, 1.00f);
		colors[ImGuiCol_Button] = ImVec4(0.21f, 0.21f, 0.21f, 0.40f);
		colors[ImGuiCol_ButtonHovered] = ImVec4(0.33f, 0.33f, 0.33f, 1.00f);
		colors[ImGuiCol_ButtonActive] = ImVec4(0.57f, 0.57f, 0.57f, 1.00f);
		colors[ImGuiCol_Header] = ImVec4(0.00f, 0.00f, 0.00f, 0.40f);
		colors[ImGuiCol_HeaderHovered] = ImVec4(0.10f, 0.10f, 0.10f, 0.80f);
		colors[ImGuiCol_HeaderActive] = ImVec4(0.26f, 0.26f, 0.26f, 1.00f);
		colors[ImGuiCol_SeparatorHovered] = ImVec4(0.53f, 0.53f, 0.53f, 0.78f);
		colors[ImGuiCol_SeparatorActive] = ImVec4(0.76f, 0.76f, 0.76f, 1.00f);
		colors[ImGuiCol_ResizeGrip] = ImVec4(0.98f, 0.98f, 0.98f, 0.20f);
		colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.72f, 0.72f, 0.72f, 0.67f);
		colors[ImGuiCol_ResizeGripActive] = ImVec4(0.92f, 0.93f, 0.93f, 0.95f);
		colors[ImGuiCol_Tab] = ImVec4(0.21f, 0.21f, 0.21f, 0.86f);
		colors[ImGuiCol_TabHovered] = ImVec4(0.43f, 0.43f, 0.43f, 0.80f);
		colors[ImGuiCol_TabActive] = ImVec4(0.35f, 0.35f, 0.35f, 1.00f);
	}
}