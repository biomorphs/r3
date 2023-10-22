#include "editor_system.h"
#include "editor/world_editor_window.h"
#include "engine/imgui_menubar_helper.h"
#include "render/render_system.h"
#include "core/profiler.h"
#include "imgui.h"
#include <format>

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

		m_allWindows.push_back(std::make_unique<WorldEditorWindow>("Benchmarks"));
		m_allWindows.push_back(std::make_unique<WorldEditorWindow>("EditorWorld"));

		return true;
	}

	void EditorSystem::Shutdown()
	{
		R3_PROF_EVENT();
	}

	void EditorSystem::ShowMainMenu()
	{
		MenuBar mainMenu;
		auto& fileMenu = mainMenu.GetSubmenu("File");
		fileMenu.AddItem("Exit", [this]() {
			m_quitRequested = true;
		});
		mainMenu.Display(true);	// true = append to main menu
	}

	void EditorSystem::ShowWindowTabs()
	{
		uint32_t windowFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar;
		auto windowFullExtents = GetSystem<RenderSystem>()->GetWindowExtents();
		ImGui::SetNextWindowPos({ 0,ImGui::GetTextLineHeightWithSpacing() + 1 });
		ImGui::SetNextWindowSize(ImVec2(windowFullExtents.x, 0));
		if (ImGui::Begin("##EditorWindowTabs", nullptr, windowFlags))
		{
			if (ImGui::BeginTabBar("Windows", ImGuiTabBarFlags_Reorderable | ImGuiTabBarFlags_FittingPolicyScroll))
			{
				for (int window = 0; window < m_allWindows.size(); window++)
				{
					std::string uniqueId = std::format("{}##{}", m_allWindows[window]->GetWindowTitle(), window);
					if (ImGui::BeginTabItem(uniqueId.c_str()))
					{
						m_selectedWindowTab = window;
						ImGui::EndTabItem();
					}
				}
				ImGui::EndTabBar();
			}
		}
		ImGui::End();
	}

	bool EditorSystem::ShowGui()
	{
		R3_PROF_EVENT();

		ApplyStyle();
		ShowMainMenu();
		ShowWindowTabs();
		
		if (m_selectedWindowTab != m_activeWindowIndex)	// window selection changed?
		{
			if (m_activeWindowIndex != -1 && m_activeWindowIndex < m_allWindows.size())
			{
				m_allWindows[m_activeWindowIndex]->OnFocusLost();
			}
			m_allWindows[m_selectedWindowTab]->OnFocusGained();
			m_activeWindowIndex = m_selectedWindowTab;
		}
		if (m_selectedWindowTab < m_allWindows.size())
		{
			m_allWindows[m_selectedWindowTab]->Update();
		}

		return !m_quitRequested;
	}

	void EditorSystem::ApplyStyle()
	{
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