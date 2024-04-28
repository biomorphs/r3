#include "static_mesh.h"
#include "engine/systems/static_mesh_system.h"

namespace R3
{
	void StaticMeshComponent::SerialiseJson(JsonSerialiser& s)
	{

	}

	void StaticMeshComponent::Inspect(const Entities::EntityHandle& e, Entities::World* w, ValueInspector& i)
	{

	}

	void StaticMeshComponent::SetModel(ModelDataHandle s)
	{
		m_modelHandle = s;
	}

	ModelDataHandle StaticMeshComponent::GetModel() const
	{
		return m_modelHandle;
	}
}