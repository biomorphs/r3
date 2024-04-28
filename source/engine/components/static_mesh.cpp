#include "static_mesh.h"
#include "engine/systems/model_data_system.h"

namespace R3
{
	void StaticMeshComponent::SerialiseJson(JsonSerialiser& s)
	{
		s("Model", m_modelHandle);
	}

	void StaticMeshComponent::Inspect(const Entities::EntityHandle& e, Entities::World* w, ValueInspector& i)
	{
		auto modelSys = Systems::GetSystem<ModelDataSystem>();
		std::string currentPath = modelSys->GetModelName(m_modelHandle);
		i.InspectFile("Model Path", currentPath, "gltf,fbx,obj,usda", InspectProperty(&StaticMeshComponent::SetModelFromPath, e, w));
	}

	void StaticMeshComponent::SetModel(ModelDataHandle s)
	{
		m_modelHandle = s;
	}

	ModelDataHandle StaticMeshComponent::GetModel() const
	{
		return m_modelHandle;
	}

	void StaticMeshComponent::SetModelFromPath(const std::string& path)
	{
		auto modelSys = Systems::GetSystem<ModelDataSystem>();
		m_modelHandle = modelSys->LoadModel(path.c_str());
	}
}