#pragma once

#include "engine/systems.h"
#include "editor/editor_window.h"
#include <set>	// questionable

namespace R3
{
	class EditorSystem : public System
	{
	public:
		static std::string_view GetName() { return "EditorSystem"; }
		virtual void RegisterTickFns();
		virtual bool Init();
		virtual void Shutdown();
		void CloseWindow(EditorWindow* window);
	private:
		void CloseAllWindows();
		void ProcessClosingWindows();
		void OnNewWorld();
		void ShowMainMenu();
		void ShowWindowTabs();
		bool ShowGui();
		void ApplyStyle();
		std::vector<std::unique_ptr<EditorWindow>> m_allWindows;
		std::set<EditorWindow*> m_windowsToClose;
		int m_selectedWindowTab = -1;
		int m_activeWindowIndex = -1;
		bool m_quitRequested = false;
		int m_worldInternalNameCounter = 0;	// used to generate internal world names for entity system
	};
}