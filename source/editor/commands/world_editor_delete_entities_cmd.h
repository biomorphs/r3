#pragma once
#include "editor/editor_command.h"
#include "entities/entity_handle.h"
#include "engine/serialiser.h"

namespace R3
{
	class WorldEditorWindow;

	// When an entity is deleted, its slot is reserved
	// on undo, we restore the previously reserved handles
	class WorldEditorDeleteEntitiesCmd : public EditorCommand
	{
	public:
		WorldEditorDeleteEntitiesCmd(WorldEditorWindow* w);
		virtual std::string_view GetName() { return "Delete entities"; }
		virtual Result Execute();
		virtual bool CanUndoRedo() { return true; }
		virtual Result Undo();
		virtual Result Redo();
	private:
		std::vector<Entities::EntityHandle> m_deletedEntities;	// keep the handles around for restoration
		std::string m_serialisedEntities;		// the serialised state of the entities we deleted
		WorldEditorWindow* m_window = nullptr;
	};
}