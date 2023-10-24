#pragma once

#include "editor/editor_command.h"
#include <vector>

namespace R3
{
	namespace Entities
	{
		class EntityHandle;
	}
	class WorldEditorWindow;

	// represents a list of entities to select and deselect
	class WorldEditorSelectEntitiesCommand : public EditorCommand
	{
	public:
		WorldEditorSelectEntitiesCommand(WorldEditorWindow* w);
		virtual ~WorldEditorSelectEntitiesCommand();
		virtual std::string_view GetName() { return "Change entity selection"; }
		virtual Result Execute();
		virtual bool CanUndoRedo() { return true; }
		virtual Result Undo();
		virtual Result Redo();

		bool m_selectAll = false;
		bool m_deselectAll = false;
		std::vector<Entities::EntityHandle> m_toSelect;
		bool m_appendToSelection = false;
		std::vector<Entities::EntityHandle> m_toDeselect;
		
	private:
		std::vector<Entities::EntityHandle> m_oldSelection;
		WorldEditorWindow* m_window;
	};
}