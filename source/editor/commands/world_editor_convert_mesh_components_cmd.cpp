#include "world_editor_convert_mesh_components_cmd.h"
#include "editor/world_editor_window.h"
#include "entities/world.h"
#include "engine/components/static_mesh.h"
#include "engine/systems/mesh_renderer.h"
#include "core/profiler.h"
#include <format>

namespace R3
{
	std::string_view WorldEditorConvertMeshComponentsCmd::GetName()
	{
		if (m_conversion == ConversionType::StaticToDynamic)
		{
			return "Convert static meshes to dynamic";
		}
		else
		{
			return "Convert dynamic meshes to static";
		}
	}

	bool WorldEditorConvertMeshComponentsCmd::ConvertEntity(Entities::EntityHandle e, ConversionType conversion)
	{
		R3_PROF_EVENT();
		auto world = m_window->GetWorld();
		StaticMeshComponent* prevStaticMesh = world->GetComponent<StaticMeshComponent>(e);
		DynamicMeshComponent* prevDynamicMesh = world->GetComponent<DynamicMeshComponent>(e);
		if (m_conversion == ConversionType::StaticToDynamic && prevStaticMesh != nullptr && prevDynamicMesh == nullptr)
		{
			world->AddComponent<DynamicMeshComponent>(e);
			DynamicMeshComponent* newCmp = world->GetComponent<DynamicMeshComponent>(e);
			newCmp->SetShouldDraw(prevStaticMesh->GetShouldDraw());
			newCmp->SetModelHandle(prevStaticMesh->GetModelHandle());
			newCmp->SetMaterialOverride(prevStaticMesh->GetMaterialOverride());
			world->RemoveComponent(e, StaticMeshComponent::GetTypeName());
			return true;
		}
		else if (m_conversion == ConversionType::DynamicToStatic && prevStaticMesh == nullptr && prevDynamicMesh != nullptr)
		{
			world->AddComponent<StaticMeshComponent>(e);
			StaticMeshComponent* newCmp = world->GetComponent<StaticMeshComponent>(e);
			newCmp->SetShouldDraw(prevDynamicMesh->GetShouldDraw());
			newCmp->SetModelHandle(prevDynamicMesh->GetModelHandle());
			newCmp->SetMaterialOverride(prevDynamicMesh->GetMaterialOverride());
			world->RemoveComponent(e, DynamicMeshComponent::GetTypeName());
			return true;
		}
		return false;
	}

	EditorCommand::Result WorldEditorConvertMeshComponentsCmd::Execute()
	{
		R3_PROF_EVENT();
		m_convertedToEntities.clear();
		for (int i = 0; i < m_targetEntities.size(); ++i)
		{
			if (ConvertEntity(m_targetEntities[i], m_conversion))
			{
				m_convertedToEntities.push_back(m_targetEntities[i]);
			}
		}
		if (m_convertedToEntities.size() > 0)
		{
			Systems::GetSystem<MeshRenderer>()->SetStaticsDirty();
			return Result::Succeeded;
		}
		else
		{
			return Result::Failed;
		}
	}

	EditorCommand::Result WorldEditorConvertMeshComponentsCmd::Undo()
	{
		R3_PROF_EVENT();
		ConversionType oppositeConversion = m_conversion == ConversionType::StaticToDynamic ? ConversionType::DynamicToStatic : ConversionType::StaticToDynamic;
		for (int i = 0; i < m_convertedToEntities.size(); ++i)
		{
			ConvertEntity(m_convertedToEntities[i], oppositeConversion);
		}
		Systems::GetSystem<MeshRenderer>()->SetStaticsDirty();
		return Result::Succeeded;
	}

	EditorCommand::Result WorldEditorConvertMeshComponentsCmd::Redo()
	{
		return Execute();
	}
}