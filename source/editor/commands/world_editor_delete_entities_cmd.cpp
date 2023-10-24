#include "world_editor_delete_entities_cmd.h"
#include "editor/world_editor_window.h"
#include "entities/world.h"

namespace R3
{
	WorldEditorDeleteEntitiesCmd::WorldEditorDeleteEntitiesCmd(WorldEditorWindow* w)
		: m_window(w), m_deletedEntityData(JsonSerialiser::Write)
	{
	}

	EditorCommand::Result WorldEditorDeleteEntitiesCmd::Execute()
	{
		auto world = m_window->GetWorld();
		m_oldSelection = m_window->GetSelectedEntities();
		if (m_deleteAllSelected)
		{
			m_deletedEntityData = world->SerialiseEntities(m_oldSelection);
			m_window->DeleteSelected();
		}
		return Result::Succeeded;
	}

	EditorCommand::Result WorldEditorDeleteEntitiesCmd::Undo()
	{
		std::vector<Entities::EntityHandle> restoredHandles = m_window->GetWorld()->SerialiseEntities(m_deletedEntityData);
		if (m_deleteAllSelected)
		{
			m_window->SelectEntities(restoredHandles);
		}
		else
		{
			m_window->SelectEntities(m_oldSelection);
		}
		return Result::Succeeded;
	}

	EditorCommand::Result WorldEditorDeleteEntitiesCmd::Redo()
	{
		return Execute();
	}
}