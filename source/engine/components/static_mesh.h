#pragma once
#include "entities/component_helpers.h"
#include "engine/systems/model_data_system.h"

namespace R3
{
	struct ModelDataHandle;

	// MeshComponent is used for both Static and Dynamic meshes
	// We keep separate component types to allow for iteration through static/dynamic objects separately
	// Using CRTP for compile-time polymorphism + explicit instantiation to avoid inlining everything

	template<class SpecialisedType>
	class MeshComponent
	{
	public:
		MeshComponent();
		~MeshComponent();
		static std::string_view GetTypeName() { return SpecialisedType::GetTypeName(); }
		static void RegisterScripts(class LuaSystem&);
		void SerialiseJson(JsonSerialiser& s);
		void Inspect(const Entities::EntityHandle& e, Entities::World* w, ValueInspector& i);

		void SetModelFromPath(std::string_view path);
		void SetModelHandle(ModelDataHandle h);
		void SetMaterialOverride(Entities::EntityHandle m);
		void SetShouldDraw(bool draw);
		inline ModelDataHandle GetModelHandle() { return m_modelHandle; }
		inline Entities::EntityHandle GetMaterialOverride() { return m_materialOverride; }
		inline bool GetShouldDraw() { return m_shouldDraw; }

	private:
		ModelDataHandle m_modelHandle;
		Entities::EntityHandle m_materialOverride;
		bool m_shouldDraw = true;
	};

	// Static mesh, intended for objects that are not modified or do not move often
	class StaticMeshComponent : public MeshComponent<StaticMeshComponent>
	{
	public:
		static std::string_view GetTypeName() { return "StaticMesh"; }
	};

	// Dynamic mesh, for objects that may change every frame
	class DynamicMeshComponent : public MeshComponent<DynamicMeshComponent>
	{
	public:
		static std::string_view GetTypeName() { return "DynamicMesh"; }
	};
}