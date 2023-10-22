#pragma once

#include "engine/systems.h"
#include "editor/editor_window.h"

namespace R3
{
	class EditorSystem : public System
	{
	public:
		static std::string_view GetName() { return "EditorSystem"; }
		virtual void RegisterTickFns();
		virtual bool Init();
		virtual void Shutdown();
	private:
		void ShowMainMenu();
		void ShowWindowTabs();
		bool ShowGui();
		void ApplyStyle();
		std::vector<std::unique_ptr<EditorWindow>> m_allWindows;
		int m_selectedWindowTab = -1;
		int m_activeWindowIndex = -1;
		bool m_quitRequested = false;
	};
}