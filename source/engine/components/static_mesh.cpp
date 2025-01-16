#include "static_mesh.h"
#include "engine/systems/model_data_system.h"
#include "engine/systems/lua_system.h"
#include "engine/file_dialogs.h"

namespace R3
{
	void StaticMeshComponent::RegisterScripts(LuaSystem& l)
	{
		l.RegisterType<StaticMeshComponent>(GetTypeName(),
			"SetModelFromPath", &StaticMeshComponent::SetModelFromPath,
			"SetMaterialOverride", &StaticMeshComponent::SetMaterialOverride,
			"m_shouldDraw", &StaticMeshComponent::m_shouldDraw
		);
	}

	void StaticMeshComponent::SerialiseJson(JsonSerialiser& s)
	{
		s("Model", m_modelHandle);
		s("MaterialsOverride", m_materialOverride);
		s("Draw", m_shouldDraw);
	}

	void StaticMeshComponent::Inspect(const Entities::EntityHandle& e, Entities::World* w, ValueInspector& i)
	{
		auto modelSys = Systems::GetSystem<ModelDataSystem>();
		std::string currentPath = modelSys->GetModelName(m_modelHandle);

		FileDialogFilter filters[] = {
			{ "Mesh Source File", "gltf,glb,fbx,obj" }
		};
		i.InspectFile("Model Path", currentPath, InspectProperty(&StaticMeshComponent::SetModelFromPath, e, w), filters, std::size(filters));
		i.InspectEntity("Material Override", m_materialOverride, w, InspectProperty(&StaticMeshComponent::m_materialOverride, e, w));
		i.Inspect("Should Draw", m_shouldDraw, InspectProperty(&StaticMeshComponent::m_shouldDraw, e, w));
	}

	void StaticMeshComponent::SetModelFromPath(std::string_view path)
	{
		auto modelSys = Systems::GetSystem<ModelDataSystem>();
		m_modelHandle = modelSys->LoadModel(path.data());
	}
}