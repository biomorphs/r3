#include "world_editor_delete_entities_cmd.h"
#include "editor/world_editor_window.h"
#include "entities/world.h"

namespace R3
{
	WorldEditorDeleteEntitiesCmd::WorldEditorDeleteEntitiesCmd(WorldEditorWindow* w)
		: m_window(w)
	{
	}

	EditorCommand::Result WorldEditorDeleteEntitiesCmd::Execute()
	{
		auto world = m_window->GetWorld();
		m_oldSelection = m_window->GetSelectedEntities();
		if (m_deleteAllSelected)
		{
			auto deletedEntityJson = world->SerialiseEntities(m_oldSelection);
			m_serialisedEntityData = deletedEntityJson.GetJson().dump();
			m_window->DeleteSelected();
		}
		return Result::Succeeded;
	}

	EditorCommand::Result WorldEditorDeleteEntitiesCmd::Undo()
	{
		auto serialiser = JsonSerialiser(JsonSerialiser::Read);
		serialiser.LoadFromString(m_serialisedEntityData);
		std::vector<Entities::EntityHandle> restoredHandles = m_window->GetWorld()->SerialiseEntities(serialiser);
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