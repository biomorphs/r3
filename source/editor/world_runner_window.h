#pragma once
#include "editor_window.h"
#include <string>

namespace R3
{
	class WorldRunnerWindow : public EditorWindow
	{
	public:
		WorldRunnerWindow(std::string worldIdentifier, std::string filePath = "");
		virtual ~WorldRunnerWindow();
		virtual std::string_view GetWindowTitle();
		virtual void Update();
		virtual CloseStatus PrepareToClose();
		virtual void OnFocusGained();
		virtual void OnFocusLost();

	private:
		std::string m_sceneFile = "";
		std::string m_worldID = "";
	};
}