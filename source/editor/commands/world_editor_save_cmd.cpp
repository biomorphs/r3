#include "world_editor_save_cmd.h"
#include "editor/world_editor_window.h"
#include "engine/file_dialogs.h"

namespace R3
{
	WorldEditorSaveCmd::WorldEditorSaveCmd(WorldEditorWindow* w, std::string targetPath)
		: m_window(w)
		, m_targetPath(targetPath)
	{
	}

	EditorCommand::Result WorldEditorSaveCmd::Execute()
	{
		std::string savePath = m_targetPath.empty() ? FileSaveDialog(m_targetPath, "scn") : m_targetPath;
		if (savePath != "")
		{
			m_window->SaveWorld(savePath);
		}
		return Result::Succeeded;
	}
}