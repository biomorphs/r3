#include "static_mesh.h"
#include "engine/systems/lua_system.h"
#include "engine/ui/file_dialogs.h"

namespace R3
{
	StaticMeshComponent::StaticMeshComponent()
	{
	}

	StaticMeshComponent::~StaticMeshComponent()
	{
	}

	void StaticMeshComponent::RegisterScripts(LuaSystem& l)
	{
		l.RegisterType<StaticMeshComponent>(GetTypeName(),
			"SetModelFromPath", &StaticMeshComponent::SetModelFromPath,
			"SetMaterialOverride", &StaticMeshComponent::SetMaterialOverride,
			"SetShouldDraw", &StaticMeshComponent::SetShouldDraw
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
		i.InspectEntity("Material Override", m_materialOverride, w, InspectProperty(&StaticMeshComponent::SetMaterialOverride, e, w));
		i.Inspect("Should Draw", m_shouldDraw, InspectProperty(&StaticMeshComponent::SetShouldDraw, e, w));
	}

	void StaticMeshComponent::SetModelHandle(ModelDataHandle h)
	{
		m_modelHandle = h;
	}

	void StaticMeshComponent::SetShouldDraw(bool draw)
	{
		m_shouldDraw = draw;
	}

	void StaticMeshComponent::SetMaterialOverride(Entities::EntityHandle m) 
	{ 
		m_materialOverride = m; 
	}

	void StaticMeshComponent::SetModelFromPath(std::string_view path)
	{
		auto modelSys = Systems::GetSystem<ModelDataSystem>();
		m_modelHandle = modelSys->LoadModel(path.data());
	}
}