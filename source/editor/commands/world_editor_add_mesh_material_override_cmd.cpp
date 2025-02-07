#include "world_editor_add_mesh_material_override_cmd.h"
#include "editor/world_editor_window.h"
#include "engine/components/static_mesh.h"
#include "engine/components/static_mesh_materials.h"
#include "entities/world.h"
#include "core/profiler.h"
#include <format>

namespace R3
{
	EditorCommand::Result WorldEditorAddMeshMaterialOverrideCommand::Execute()
	{
		R3_PROF_EVENT();
		auto world = m_window->GetWorld();
		for (int i = 0; i < m_targetEntities.size(); ++i)
		{
			world->AddComponent<StaticMeshMaterialsComponent>(m_targetEntities[i]);
			if (auto matCmp = world->GetComponent<StaticMeshMaterialsComponent>(m_targetEntities[i]))
			{
				matCmp->PopulateFromMesh(m_targetEntities[i], world);
			}
			if (auto mesh = world->GetComponent<StaticMeshComponent>(m_targetEntities[i]))
			{
				mesh->SetMaterialOverride(m_targetEntities[i]);
			}
			if (auto mesh = world->GetComponent<DynamicMeshComponent>(m_targetEntities[i]))
			{
				mesh->SetMaterialOverride(m_targetEntities[i]);
			}
		}
		return Result();
	}

	EditorCommand::Result WorldEditorAddMeshMaterialOverrideCommand::Undo()
	{
		R3_PROF_EVENT();
		auto world = m_window->GetWorld();
		for (int i = 0; i < m_targetEntities.size(); ++i)
		{
			world->RemoveComponent(m_targetEntities[i], StaticMeshMaterialsComponent::GetTypeName());
			if (auto mesh = world->GetComponent<StaticMeshComponent>(m_targetEntities[i]))
			{
				mesh->SetMaterialOverride({});
			}
			if (auto mesh = world->GetComponent<DynamicMeshComponent>(m_targetEntities[i]))
			{
				mesh->SetMaterialOverride({});
			}
		}
		return Result();
	}

	EditorCommand::Result WorldEditorAddMeshMaterialOverrideCommand::Redo()
	{
		return Execute();
	}
}