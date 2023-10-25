#pragma once
#include "editor/editor_command.h"
#include "entities/entity_handle.h"
#include "engine/serialiser.h"

namespace R3
{
	class WorldEditorWindow;

	class WorldEditorDeleteEntitiesCmd : public EditorCommand
	{
	public:
		WorldEditorDeleteEntitiesCmd(WorldEditorWindow* w);
		virtual std::string_view GetName() { return "Delete entities"; }
		virtual Result Execute();
		virtual bool CanUndoRedo() { return true; }
		virtual Result Undo();
		virtual Result Redo();

		bool m_deleteAllSelected = false;
	private:
		std::vector<Entities::EntityHandle> m_oldSelection;
		std::string m_serialisedEntityData;	// represents the deleted entities as json
		WorldEditorWindow* m_window = nullptr;
	};
}