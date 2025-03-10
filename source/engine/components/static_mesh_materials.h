#pragma once

#include "entities/component_helpers.h"
#include "engine/systems/static_mesh_system.h"	// gross
#include <vector>

namespace R3
{
	struct MeshMaterial;
	class StaticMeshMaterialsComponent
	{
	public:
		StaticMeshMaterialsComponent();
		virtual ~StaticMeshMaterialsComponent();
		static std::string_view GetTypeName() { return "StaticMeshMaterials"; }
		void SerialiseJson(JsonSerialiser& s);
		void Inspect(const Entities::EntityHandle& e, Entities::World* w, ValueInspector& i);
		void PopulateFromMesh(const Entities::EntityHandle& e, Entities::World* w);

		std::vector<MeshMaterial> m_materials;
		uint32_t m_gpuDataIndex = -1;	// indexes into StaticMeshSystem::m_allMaterialsGpu
	};
}