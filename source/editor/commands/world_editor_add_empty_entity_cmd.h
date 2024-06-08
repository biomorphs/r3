#pragma once

#include "editor/editor_command.h"
#include "entities/entity_handle.h"
#include <vector>

namespace R3
{
	class WorldEditorWindow;

	// create an entity, call a user callback on creation
	class WorldEditorAddEmptyEntityCommand : public EditorCommand
	{
	public:
		WorldEditorAddEmptyEntityCommand(WorldEditorWindow* w, std::string name = "") : m_window(w), m_newName(name) {}
		virtual std::string_view GetName() { return "Add Empty Entity"; }
		virtual Result Execute();
		virtual bool CanUndoRedo() { return true; }
		virtual Result Undo();
		virtual Result Redo();

		using OnEntityCreated = std::function<void(const Entities::EntityHandle&)>;
		void SetOnCreate(OnEntityCreated fn);

	private:
		std::string m_newName;
		Entities::EntityHandle m_createdEntity;
		std::vector<Entities::EntityHandle> m_oldSelection;
		WorldEditorWindow* m_window = nullptr;
		OnEntityCreated m_onEntityCreated;
	};
}