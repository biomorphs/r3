#include "static_mesh.h"
#include "engine/systems/lua_system.h"
#include "engine/ui/file_dialogs.h"

namespace R3
{
	template<class SpecialisedType>
	MeshComponent<SpecialisedType>::MeshComponent()
	{
	}

	template<class SpecialisedType>
	MeshComponent<SpecialisedType>::~MeshComponent()
	{
	}

	template<class SpecialisedType>
	void MeshComponent<SpecialisedType>::RegisterScripts(LuaSystem& l)
	{
		l.RegisterType<SpecialisedType>(GetTypeName(),
			"SetModelFromPath", &SpecialisedType::SetModelFromPath,
			"SetMaterialOverride", &SpecialisedType::SetMaterialOverride,
			"SetShouldDraw", &SpecialisedType::SetShouldDraw
		);
	}

	template<class SpecialisedType>
	void MeshComponent<SpecialisedType>::SerialiseJson(JsonSerialiser& s)
	{
		s("Model", m_modelHandle);
		s("MaterialsOverride", m_materialOverride);
		s("Draw", m_shouldDraw);
	}

	template<class SpecialisedType>
	void MeshComponent<SpecialisedType>::Inspect(const Entities::EntityHandle& e, Entities::World* w, ValueInspector& i)
	{
		auto modelSys = Systems::GetSystem<ModelDataSystem>();
		std::string currentPath = modelSys->GetModelName(m_modelHandle);

		FileDialogFilter filters[] = {
			{ "Mesh Source File", "gltf,glb,fbx,obj" }
		};
		// note, we must explicitly call InspectPropery<SpecialisedType> to ensure the specialised component type is used
		i.InspectFile("Model Path", currentPath, InspectProperty<SpecialisedType>(&SpecialisedType::SetModelFromPath, e, w), filters, std::size(filters));
		i.InspectEntity("Material Override", m_materialOverride, w, InspectProperty<SpecialisedType>(&SpecialisedType::SetMaterialOverride, e, w));
		i.Inspect("Should Draw", m_shouldDraw, InspectProperty<SpecialisedType>(&SpecialisedType::SetShouldDraw, e, w));
	}

	template<class SpecialisedType>
	void MeshComponent<SpecialisedType>::SetModelHandle(ModelDataHandle h)
	{
		m_modelHandle = h;
	}

	template<class SpecialisedType>
	void MeshComponent<SpecialisedType>::SetShouldDraw(bool draw)
	{
		m_shouldDraw = draw;
	}

	template<class SpecialisedType>
	void MeshComponent<SpecialisedType>::SetMaterialOverride(Entities::EntityHandle m)
	{ 
		m_materialOverride = m; 
	}

	template<class SpecialisedType>
	void MeshComponent<SpecialisedType>::SetModelFromPath(std::string_view path)
	{
		auto modelSys = Systems::GetSystem<ModelDataSystem>();
		m_modelHandle = modelSys->LoadModel(path.data());
	}

	// Explicit instantiation for linking 
	template class MeshComponent<StaticMeshComponent>;
	template class MeshComponent<DynamicMeshComponent>;
}