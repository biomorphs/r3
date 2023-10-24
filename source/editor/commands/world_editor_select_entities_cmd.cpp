#include "world_editor_select_entities_cmd.h"
#include "editor/world_editor_window.h"
#include "core/profiler.h"

namespace R3
{
	WorldEditorSelectEntitiesCommand::WorldEditorSelectEntitiesCommand(WorldEditorWindow* w)
		: m_window(w)
	{
	}

	WorldEditorSelectEntitiesCommand::~WorldEditorSelectEntitiesCommand()
	{
	}

	EditorCommand::Result WorldEditorSelectEntitiesCommand::Execute()
	{
		m_oldSelection = m_window->GetSelectedEntities();
		if (m_selectAll)
		{
			m_window->SelectAll();
		}
		if (m_deselectAll || (m_toSelect.size() > 0 && !m_appendToSelection))
		{
			m_window->DeselectAll();
		}
		for (const auto& toSelect : m_toSelect)
		{
			m_window->SelectEntity(toSelect);
		}
		for (const auto& toDeselect : m_toDeselect)
		{
			m_window->DeselectEntity(toDeselect);
		}
		return EditorCommand::Result::Succeeded;
	}

	EditorCommand::Result WorldEditorSelectEntitiesCommand::Undo()
	{
		R3_PROF_EVENT();
		m_window->DeselectAll();
		m_window->SelectEntities(m_oldSelection);
		return EditorCommand::Result::Succeeded;
	}

	EditorCommand::Result WorldEditorSelectEntitiesCommand::Redo()
	{
		return Execute();
	}


}