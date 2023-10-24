#pragma once

#include "editor/editor_command.h"
#include "entities/entity_handle.h"
#include <vector>

namespace R3
{
	class WorldEditorWindow;

	// represents a list of entities to select and deselect
	class WorldEditorAddEmptyEntityCommand : public EditorCommand
	{
	public:
		WorldEditorAddEmptyEntityCommand(WorldEditorWindow* w) : m_window(w) {}
		virtual std::string_view GetName() { return "Add Empty Entity"; }
		virtual Result Execute();
		virtual bool CanUndoRedo() { return true; }
		virtual Result Undo();
		virtual Result Redo();
	private:
		Entities::EntityHandle m_createdEntity;
		std::vector<Entities::EntityHandle> m_oldSelection;
		WorldEditorWindow* m_window = nullptr;
	};
}