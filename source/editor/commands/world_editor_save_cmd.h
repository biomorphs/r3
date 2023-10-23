#pragma once
#include "editor/editor_command.h"

namespace R3
{
	class WorldEditorWindow;
	class WorldEditorSaveCmd : public EditorCommand
	{
	public:
		WorldEditorSaveCmd(WorldEditorWindow* w, std::string targetPath);
		virtual std::string_view GetName() { return "Save World"; }
		virtual Result Execute();
		virtual bool CanUndoRedo() { return false; }
	private:
		WorldEditorWindow* m_window = nullptr;
		std::string m_targetPath;
	};
}