#include "static_mesh_materials.h"
#include "engine/systems/static_mesh_system.h"
#include "engine/components/static_mesh.h"
#include "engine/systems/model_data_system.h"
#include "engine/model_data.h"
#include <imgui.h>

namespace R3
{
	void StaticMeshMaterialsComponent::MaterialOverride::SerialiseJson(JsonSerialiser& s)
	{
		s("Albedo", m_albedo);
		s("Opacity", m_opacity);
		s("Metallic", m_metallic);
		s("Roughness", m_roughness);
	}

	StaticMeshMaterialsComponent::StaticMeshMaterialsComponent()
	{
	}

	StaticMeshMaterialsComponent::~StaticMeshMaterialsComponent()
	{
	}

	void StaticMeshMaterialsComponent::SerialiseJson(JsonSerialiser& s)
	{
		s("Materials", m_materials);
	}

	void StaticMeshMaterialsComponent::Inspect(const Entities::EntityHandle& e, Entities::World* w, ValueInspector& i)
	{
		// Todo - add undo/redo friendly UI
		if (ImGui::Button("Populate from Static Mesh"))
		{
			auto staticMeshCmp = w->GetComponent<StaticMeshComponent>(e);
			auto mds = Systems::GetSystem<ModelDataSystem>();
			if (staticMeshCmp && staticMeshCmp->GetModel().m_index != -1)
			{
				auto modelData = mds->GetModelData(staticMeshCmp->GetModel());
				auto& srcMats = modelData.m_data->m_materials;
				m_materials.clear();
				for (int m = 0; m < srcMats.size(); ++m)
				{
					MaterialOverride newMat;
					newMat.m_albedo = srcMats[m].m_albedo;
					newMat.m_opacity = srcMats[m].m_opacity;
					newMat.m_metallic = srcMats[m].m_metallic;
					newMat.m_roughness = srcMats[m].m_roughness;
					m_materials.push_back(newMat);
				}
			}
		}

		for (int m = 0; m < m_materials.size(); ++m)
		{
			auto inspectAlbedo = InspectComponentCustom<StaticMeshMaterialsComponent, glm::vec4>
				(e, w, [m](const Entities::EntityHandle&, StaticMeshMaterialsComponent& cmp, Entities::World*, glm::vec4 v)
			{
				if (m < cmp.m_materials.size())
				{
					cmp.m_materials[m].m_albedo = glm::vec3(v);
				}
			});
			auto inspectOpacity = InspectComponentCustom<StaticMeshMaterialsComponent, float>
				(e, w, [m](const Entities::EntityHandle&, StaticMeshMaterialsComponent& cmp, Entities::World*, float v) {
					if (m < cmp.m_materials.size())
					{
						cmp.m_materials[m].m_opacity = v;
					}
			});
			auto inspectMetallic = InspectComponentCustom<StaticMeshMaterialsComponent, float>
				(e, w, [m](const Entities::EntityHandle&, StaticMeshMaterialsComponent& cmp, Entities::World*, float v) {
					if (m < cmp.m_materials.size())
					{
						cmp.m_materials[m].m_metallic = v;
					}
			});
			auto inspectRoughness = InspectComponentCustom<StaticMeshMaterialsComponent, float>
				(e, w, [m](const Entities::EntityHandle&, StaticMeshMaterialsComponent& cmp, Entities::World*, float v) {
					if (m < cmp.m_materials.size())
					{
						cmp.m_materials[m].m_roughness = v;
					}
			});
			i.InspectColour(std::format("Albedo##{}", m), { m_materials[m].m_albedo, 1.0f }, inspectAlbedo);
			i.Inspect(std::format("Albedo##{}", m), m_materials[m].m_opacity, inspectOpacity, 0.01f, 0.0f, 1.0f);
			i.Inspect(std::format("Metallic##{}", m), m_materials[m].m_metallic, inspectMetallic, 0.01f, 0.0f, 1.0f);
			i.Inspect(std::format("Roughness##{}", m), m_materials[m].m_roughness, inspectRoughness, 0.01f, 0.0f, 1.0f);
		}
	}

}