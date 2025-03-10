#include "world_editor_save_cmd.h"
#include "editor/world_editor_window.h"
#include "engine/ui/file_dialogs.h"
#include "core/profiler.h"

namespace R3
{
	WorldEditorSaveCmd::WorldEditorSaveCmd(WorldEditorWindow* w, std::string targetPath)
		: m_window(w)
		, m_targetPath(targetPath)
	{
	}

	EditorCommand::Result WorldEditorSaveCmd::Execute()
	{
		R3_PROF_EVENT();
		FileDialogFilter filters[] = {
			{ "Scene File", "scn" }
		};
		std::string savePath = m_targetPath.empty() ? FileSaveDialog(m_targetPath, filters, std::size(filters)) : m_targetPath;
		if (savePath != "")
		{
			m_window->SaveWorld(savePath);
		}
		return Result::Succeeded;
	}
}