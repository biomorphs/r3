#include "world_editor_add_empty_entity_cmd.h"
#include "editor/world_editor_window.h"
#include "entities/world.h"
#include "core/profiler.h"

namespace R3
{
	EditorCommand::Result WorldEditorAddEmptyEntityCommand::Execute()
	{
		R3_PROF_EVENT();
		auto world = m_window->GetWorld();
		m_oldSelection = m_window->GetSelectedEntities();
		if (m_createdEntity.GetID() != -1)
		{
			m_createdEntity = world->AddEntityFromHandle(m_createdEntity);
		}
		else
		{
			m_createdEntity = world->AddEntity();
		}
		m_window->DeselectAll();
		m_window->SelectEntity(m_createdEntity);
		return EditorCommand::Result::Succeeded;
	}

	EditorCommand::Result WorldEditorAddEmptyEntityCommand::Undo()
	{
		R3_PROF_EVENT();
		auto world = m_window->GetWorld();
		world->RemoveEntity(m_createdEntity, true);	// true = reserve the handle/slot in case we need to restore it
		m_window->DeselectAll();
		m_window->SelectEntities(m_oldSelection);
		return EditorCommand::Result::Succeeded;
	}

	EditorCommand::Result WorldEditorAddEmptyEntityCommand::Redo()
	{
		return Execute();
	}


}