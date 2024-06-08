#pragma once
#include "editor/editor_command.h"
#include "entities/entity_handle.h"
#include "engine/serialiser.h"

namespace R3
{
	class WorldEditorWindow;
	class WorldEditorImportSceneCmd : public EditorCommand
	{
	public:
		WorldEditorImportSceneCmd(WorldEditorWindow* w, std::string path);
		virtual std::string_view GetName() { return "Import Scene"; }
		virtual Result Execute();
		virtual bool CanUndoRedo() { return true; }
		virtual Result Undo();
		virtual Result Redo();
	private:
		std::string m_pathToLoad;
		std::vector<Entities::EntityHandle> m_selectedEntities;	// selection before import (for undo)
		std::vector<Entities::EntityHandle> m_newEntities;		// the entities that were cloned + the root
		WorldEditorWindow* m_window = nullptr;
	};
}