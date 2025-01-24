#include "world_editor_add_entity_from_mesh_cmd.h"
#include "editor/world_editor_window.h"
#include "entities/world.h"
#include "engine/systems/static_mesh_renderer.h"
#include "core/profiler.h"
#include "engine/components/transform.h"
#include "engine/components/static_mesh.h"
#include "engine/systems/model_data_system.h"
#include <format>

namespace R3
{
	WorldEditorAddEntityFromMeshCommand::WorldEditorAddEntityFromMeshCommand(WorldEditorWindow* w, std::string_view path)
		: m_window(w)
		, m_meshPath(path)
	{
		m_cmdName = std::format("Add Entity from mesh {}", m_meshPath);
	}

	EditorCommand::Result WorldEditorAddEntityFromMeshCommand::Execute()
	{
		R3_PROF_EVENT();

		if (m_currentStatus == EditorCommand::Result::Failed)
		{
			return m_currentStatus;
		}

		auto models = Systems::GetSystem<ModelDataSystem>();
		auto theModel = models->LoadModel(m_meshPath.c_str());
		if (theModel.m_index == -1)
		{
			m_currentStatus = EditorCommand::Result::Failed;
		}
		else
		{
			auto modelData = models->GetModelData(theModel);
			if (modelData.m_data)
			{
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
				if (m_createdEntity.GetID() != -1)
				{
					std::filesystem::path filePath(m_meshPath);
					world->SetEntityName(m_createdEntity, filePath.filename().string());
					world->AddComponent<TransformComponent>(m_createdEntity);
					world->AddComponent<StaticMeshComponent>(m_createdEntity);
					StaticMeshComponent* smc = world->GetComponent<StaticMeshComponent>(m_createdEntity);
					if (smc)
					{
						smc->SetModelHandle(theModel);
					}
				}
				m_window->DeselectAll();
				m_window->SelectEntity(m_createdEntity);
				m_currentStatus = EditorCommand::Result::Succeeded;
			}
			Systems::GetSystem<StaticMeshRenderer>()->SetStaticsDirty();
		}
		
		return m_currentStatus;
	}

	EditorCommand::Result WorldEditorAddEntityFromMeshCommand::Undo()
	{
		R3_PROF_EVENT();
		auto world = m_window->GetWorld();
		world->RemoveEntity(m_createdEntity, true);	// true = reserve the handle/slot in case we need to restore it
		m_window->DeselectAll();
		m_window->SelectEntities(m_oldSelection);
		Systems::GetSystem<StaticMeshRenderer>()->SetStaticsDirty();
		return EditorCommand::Result::Succeeded;
	}

	EditorCommand::Result WorldEditorAddEntityFromMeshCommand::Redo()
	{
		return Execute();
	}


}