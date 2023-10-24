#include "world_editor_add_empty_entity_cmd.h"
#include "editor/world_editor_window.h"
#include "entities/world.h"

namespace R3
{
	EditorCommand::Result WorldEditorAddEmptyEntityCommand::Execute()
	{
		auto world = m_window->GetWorld();
		m_oldSelection = m_window->GetSelectedEntities();
		m_createdEntity = world->AddEntity();
		m_window->DeselectAll();
		m_window->SelectEntity(m_createdEntity);
		return EditorCommand::Result::Succeeded;
	}

	EditorCommand::Result WorldEditorAddEmptyEntityCommand::Undo()
	{
		auto world = m_window->GetWorld();
		world->RemoveEntity(m_createdEntity);
		m_createdEntity = {};
		m_window->DeselectAll();
		m_window->SelectEntities(m_oldSelection);
		return EditorCommand::Result::Succeeded;
	}

	EditorCommand::Result WorldEditorAddEmptyEntityCommand::Redo()
	{
		return Execute();
	}


}