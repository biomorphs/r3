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
		m_deletedEntities = m_window->GetSelectedEntities();
		m_window->DeselectAll();
		JsonSerialiser json = world->SerialiseEntities(m_deletedEntities);
		m_serialisedEntities = json.GetJson().dump();
		for (int i = 0; i < m_deletedEntities.size(); ++i)
		{
			world->RemoveEntity(m_deletedEntities[i], true);	// true = reserve this handle/slot for later
		}
		return Result::Succeeded;
	}

	EditorCommand::Result WorldEditorDeleteEntitiesCmd::Undo()
	{
		auto world = m_window->GetWorld();
		JsonSerialiser json(JsonSerialiser::Read);
		json.LoadFromString(m_serialisedEntities);
		if (json.GetJson().size() != m_deletedEntities.size())
		{
			return Result::Failed;
		}
		world->SerialiseEntities(json, m_deletedEntities);	// this will restore the old handles (or fail!)
		m_window->SelectEntities(m_deletedEntities);

		return Result::Succeeded;
	}
	EditorCommand::Result WorldEditorDeleteEntitiesCmd::Redo()
	{
		return Execute();
	}
}