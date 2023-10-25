#pragma once
#include "editor/editor_command.h"
#include "entities/entity_handle.h"
#include "engine/serialiser.h"

namespace R3
{
	class WorldEditorWindow;

	// There is basically no clean way to recreate identical entity handles after they are deleted
	// As a result, deleting an entity invalidates all handles to that entity, This includes handles in other commands!
	// We cannot force the exact handle recreation since another entity may now be using that index
	// (we could try to restore all the handles anyway but it is error-prone and unpredictable)
	// It is also a massive pain to repatch all the handles, so screw it. No undo/redo for entities
	class WorldEditorDeleteEntitiesCmd : public EditorCommand
	{
	public:
		WorldEditorDeleteEntitiesCmd(WorldEditorWindow* w);
		virtual std::string_view GetName() { return "Delete entities"; }
		virtual Result Execute();
		virtual bool CanUndoRedo() { return false; }
	private:
		WorldEditorWindow* m_window = nullptr;
		bool m_openedPopup = false;
	};
}