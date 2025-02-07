#pragma once

#include "editor/editor_command.h"
#include "entities/entity_handle.h"
#include <vector>

namespace R3
{
	// adds a component of a particular type to one or more entities
	class WorldEditorWindow;
	class WorldEditorAddMeshMaterialOverrideCommand : public EditorCommand
	{
	public:
		WorldEditorAddMeshMaterialOverrideCommand(WorldEditorWindow* w, const std::vector<Entities::EntityHandle>& entities) : m_window(w), m_targetEntities(entities) {}
		virtual std::string_view GetName() { return "Add Material Override"; }
		virtual Result Execute();
		virtual bool CanUndoRedo() { return true; }
		virtual Result Undo();
		virtual Result Redo();

		std::vector<Entities::EntityHandle> m_targetEntities;
	private:
		WorldEditorWindow* m_window = nullptr;
	};
}