#pragma once

#include "editor/editor_command.h"
#include "entities/entity_handle.h"
#include <vector>

namespace R3
{
	class WorldEditorWindow;

	// represents a list of entities to select and deselect
	class WorldEditorAddEntityFromMeshCommand : public EditorCommand
	{
	public:
		WorldEditorAddEntityFromMeshCommand(WorldEditorWindow* w, std::string_view path);
		virtual std::string_view GetName() { return m_cmdName; }
		virtual Result Execute();
		virtual bool CanUndoRedo() { return true; }
		virtual Result Undo();
		virtual Result Redo();
	private:
		std::string m_meshPath;
		std::string m_cmdName;
		Entities::EntityHandle m_createdEntity;
		std::vector<Entities::EntityHandle> m_oldSelection;
		WorldEditorWindow* m_window = nullptr;
		EditorCommand::Result m_currentStatus = EditorCommand::Result::Waiting;
	};
}