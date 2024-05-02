#pragma once

#include "entities/component_helpers.h"
#include <vector>

namespace R3
{
	class StaticMeshMaterialsComponent
	{
	public:
		StaticMeshMaterialsComponent();
		virtual ~StaticMeshMaterialsComponent();
		static std::string_view GetTypeName() { return "Static Mesh Materials"; }
		void SerialiseJson(JsonSerialiser& s);
		void Inspect(const Entities::EntityHandle& e, Entities::World* w, ValueInspector& i);

		struct MaterialOverride {	// 1 per model in a static mesh
			void SerialiseJson(JsonSerialiser& s);
			glm::vec3 m_albedo;
			float m_opacity;
			float m_metallic;
			float m_roughness;
		};
		std::vector<MaterialOverride> m_materials;
	};
}