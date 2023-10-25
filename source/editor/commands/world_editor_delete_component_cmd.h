#pragma once

#include "editor/editor_command.h"
#include "entities/entity_handle.h"
#include <vector>

namespace R3
{
	// delete a component of a particular type from one or more entities
	class WorldEditorWindow;
	class WorldEditorDeleteComponentCommand : public EditorCommand
	{
	public:
		WorldEditorDeleteComponentCommand(WorldEditorWindow* w) : m_window(w) {}
		virtual std::string_view GetName();
		virtual Result Execute();
		virtual bool CanUndoRedo() { return true; }
		virtual Result Undo();
		virtual Result Redo();

		std::vector<Entities::EntityHandle> m_targetEntities;
		std::string m_componentType;
	private:
		WorldEditorWindow* m_window = nullptr;
		std::vector<Entities::EntityHandle> m_deletedFromEntities;	// to undo we need to know which entities were modified
		std::vector<std::string> m_serialisedComponents;			// for each entity, we store the serialised data of the deleted components
	};
}