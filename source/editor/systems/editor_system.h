#pragma once

#include "engine/systems.h"
#include "editor/editor_window.h"
#include <set>	// questionable

namespace R3
{
	class ConsoleLogWidget;
	class EditorSystem : public System
	{
	public:
		EditorSystem();
		virtual ~EditorSystem();
		static std::string_view GetName() { return "EditorSystem"; }
		virtual void RegisterTickFns();
		virtual bool Init();
		virtual void Shutdown();
		void CloseWindow(EditorWindow* window);
		void RunWorld(std::string path);
	private:
		void CloseAllWindows();
		void ProcessClosingWindows();
		void OnNewWorld();
		void OnOpenWorld();
		void OnRunWorld();
		void ShowMainMenu();
		void ShowWindowTabs();
		bool ShowGui();
		void ApplyStyle();
		std::vector<std::unique_ptr<EditorWindow>> m_allWindows;
		std::set<EditorWindow*> m_windowsToClose;
		std::unique_ptr<ConsoleLogWidget> m_consoleLogWidget;
		int m_selectedWindowTab = -1;
		int m_activeWindowIndex = -1;
		bool m_quitRequested = false;
		int m_worldInternalNameCounter = 0;	// used to generate internal world names for entity system
	};
}