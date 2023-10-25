#pragma once

#include "editor/editor_command.h"
#include "entities/entity_handle.h"
#include <vector>

namespace R3
{
	// adds a component of a particular type to one or more entities
	class WorldEditorWindow;
	class WorldEditorAddComponentCommand : public EditorCommand
	{
	public:
		WorldEditorAddComponentCommand(WorldEditorWindow* w) : m_window(w) {}
		virtual std::string_view GetName();
		virtual Result Execute();
		virtual bool CanUndoRedo() { return true; }
		virtual Result Undo();
		virtual Result Redo();

		std::vector<Entities::EntityHandle> m_targetEntities;
		std::string m_componentType;
	private:
		WorldEditorWindow* m_window = nullptr;
		std::vector<Entities::EntityHandle> m_addedToEntities;	// to undo we need to know which entities got a new component
	};
}