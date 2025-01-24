#include "world_editor_set_entity_parent_cmd.h"
#include "editor/world_editor_window.h"
#include "entities/world.h"
#include "engine/systems/static_mesh_renderer.h"
#include "core/profiler.h"

namespace R3
{

	WorldEditorSetEntityParentCmd::WorldEditorSetEntityParentCmd(WorldEditorWindow* w, Entities::EntityHandle newParent)
		: m_window(w)
		, m_newParent(newParent)
	{
	}

	EditorCommand::Result WorldEditorSetEntityParentCmd::Execute()
	{
		R3_PROF_EVENT();
		m_children = m_window->GetSelectedEntities();
		m_oldParents.resize(m_children.size());
		for (int i = 0; i < m_children.size(); ++i)
		{
			m_oldParents[i] = m_window->GetWorld()->GetParent(m_children[i]);
			m_window->GetWorld()->SetParent(m_children[i], m_newParent);
		}
		Systems::GetSystem<MeshRenderer>()->SetStaticsDirty();
		return EditorCommand::Result::Succeeded;
	}

	EditorCommand::Result WorldEditorSetEntityParentCmd::Undo()
	{
		R3_PROF_EVENT();
		for (int i = 0; i < m_children.size(); ++i)
		{
			m_window->GetWorld()->SetParent(m_children[i], m_oldParents[i]);
		}
		Systems::GetSystem<MeshRenderer>()->SetStaticsDirty();
		return EditorCommand::Result::Succeeded;
	}

	EditorCommand::Result WorldEditorSetEntityParentCmd::Redo()
	{
		return Execute();
	}
}