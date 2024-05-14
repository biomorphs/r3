#pragma once

#include "editor/editor_command.h"
#include "entities/entity_handle.h"

namespace R3
{
	class WorldEditorWindow;

	class WorldEditorSetEntityParentCmd : public EditorCommand
	{
	public:
		WorldEditorSetEntityParentCmd(WorldEditorWindow* w, Entities::EntityHandle newParent);
		virtual std::string_view GetName() { return "Set entity parent"; }
		virtual Result Execute();
		virtual bool CanUndoRedo() { return true; }
		virtual Result Undo();
		virtual Result Redo();
	private:
		std::vector<Entities::EntityHandle> m_children;			// we will set all their parents
		std::vector<Entities::EntityHandle> m_oldParents;		// for undo/redo
		Entities::EntityHandle m_newParent;
		WorldEditorWindow* m_window = nullptr;
	};
}