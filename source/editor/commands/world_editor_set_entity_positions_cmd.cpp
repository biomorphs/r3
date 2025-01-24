#include "world_editor_set_entity_positions_cmd.h"
#include "editor/world_editor_window.h"
#include "engine/components/transform.h"
#include "engine/systems/mesh_renderer.h"

namespace R3
{
	EditorCommand::Result WorldEditorSetEntityPositionsCommand::Redo()
	{
		return Execute();
	}

	EditorCommand::Result WorldEditorSetEntityPositionsCommand::Undo()
	{
		for (auto& ent : m_entities)
		{
			auto transform = m_window->GetWorld()->GetComponent<TransformComponent>(ent.m_entity);
			if (transform)
			{
				transform->SetPositionWorldSpaceNoInterpolation(ent.m_entity, *m_window->GetWorld(), ent.m_oldPos);
			}
		}
		Systems::GetSystem<MeshRenderer>()->SetStaticsDirty();
		return EditorCommand::Result::Succeeded;
	}

	WorldEditorSetEntityPositionsCommand::WorldEditorSetEntityPositionsCommand(WorldEditorWindow* w)
		: m_window(w)
	{
	}

	EditorCommand::Result WorldEditorSetEntityPositionsCommand::Execute()
	{
		for (auto& ent : m_entities)
		{
			auto transform = m_window->GetWorld()->GetComponent<TransformComponent>(ent.m_entity);
			if (transform)
			{
				transform->SetPositionWorldSpaceNoInterpolation(ent.m_entity, *m_window->GetWorld(), ent.m_newPos);
			}
		}
		Systems::GetSystem<MeshRenderer>()->SetStaticsDirty();
		return EditorCommand::Result::Succeeded;
	}
}