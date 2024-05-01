#pragma once
#include "editor/editor_command.h"
#include "entities/entity_handle.h"
#include "engine/serialiser.h"

namespace R3
{
	class WorldEditorWindow;

	class WorldEditorCloneEntitiesCmd : public EditorCommand
	{
	public:
		WorldEditorCloneEntitiesCmd(WorldEditorWindow* w);
		virtual std::string_view GetName() { return "Clone entities"; }
		virtual Result Execute();
		virtual bool CanUndoRedo() { return true; }
		virtual Result Undo();
		virtual Result Redo();
	private:
		std::vector<Entities::EntityHandle> m_selectedEntities;		// the entities that were cloned
		std::vector<Entities::EntityHandle> m_newIDs;				// the clones
		WorldEditorWindow* m_window = nullptr;
	};
}