#include "world_editor_clone_entities_cmd.h"
#include "editor/world_editor_window.h"
#include "engine/systems/static_mesh_renderer.h"
#include "core/profiler.h"
#include "entities/world.h"

namespace R3
{
	WorldEditorCloneEntitiesCmd::WorldEditorCloneEntitiesCmd(WorldEditorWindow* w)
		: m_window(w)
	{
	}

	EditorCommand::Result WorldEditorCloneEntitiesCmd::Execute()
	{
		R3_PROF_EVENT();
		auto world = m_window->GetWorld();
		m_selectedEntities = m_window->GetSelectedEntities();
		m_window->DeselectAll();
		JsonSerialiser json = world->SerialiseEntities(m_selectedEntities);	// clone via serialisation
		m_newIDs = world->SerialiseEntities(json);
		for (const auto it : m_newIDs)	// select the new clones
		{
			m_window->SelectEntity(it);
		}
		Systems::GetSystem<StaticMeshRenderer>()->SetStaticsDirty();
		return Result::Succeeded;
	}

	EditorCommand::Result WorldEditorCloneEntitiesCmd::Undo()
	{
		R3_PROF_EVENT();
		auto world = m_window->GetWorld();
		m_window->DeselectAll();
		for (const auto it : m_newIDs)	// delete the clone
		{
			world->RemoveEntity(it);
		}
		for (const auto it : m_selectedEntities)	// reselect the originals
		{
			m_window->SelectEntity(it);
		}
		Systems::GetSystem<StaticMeshRenderer>()->SetStaticsDirty();
		return Result::Succeeded;
	}

	EditorCommand::Result WorldEditorCloneEntitiesCmd::Redo()
	{
		return Execute();
	}
}