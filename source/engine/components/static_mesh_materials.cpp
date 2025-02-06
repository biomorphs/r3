#include "static_mesh_materials.h"
#include "engine/components/static_mesh.h"
#include "engine/systems/texture_system.h"
#include "engine/systems/model_data_system.h"
#include "engine/systems/static_mesh_system.h"
#include "engine/assets/model_data.h"
#include <imgui.h>

namespace R3
{
	void SerialiseJson(MeshMaterial& smm, JsonSerialiser& s)
	{
		auto doTexture = [&](const char* name, uint32_t& i) {
			TextureHandle th = {i};
			s(name, th);
			i = th.m_index;
		};
		s("AlbedoOpacity", smm.m_albedoOpacity);
		s("Metallic", smm.m_metallic);
		s("Roughness", smm.m_roughness);
		s("UVOffsetScale", smm.m_uvOffsetScale);
		s("Flags", smm.m_flags);
		doTexture("AlbedoTex", smm.m_albedoTexture);
		doTexture("RoughnessTex", smm.m_roughnessTexture);
		doTexture("MetalTex", smm.m_metalnessTexture);
		doTexture("NormalTex", smm.m_normalTexture);
		doTexture("AOTex", smm.m_aoTexture);
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

	void StaticMeshMaterialsComponent::PopulateFromMesh(const Entities::EntityHandle& e, Entities::World* w)
	{
		auto staticMeshCmp = w->GetComponent<StaticMeshComponent>(e);
		auto mds = Systems::GetSystem<ModelDataSystem>();
		auto textures = Systems::GetSystem<TextureSystem>();
		if (staticMeshCmp && staticMeshCmp->GetModelHandle().m_index != -1)
		{
			auto modelData = mds->GetModelData(staticMeshCmp->GetModelHandle());
			auto& srcMats = modelData.m_data->m_materials;
			m_materials.clear();
			for (int m = 0; m < srcMats.size(); ++m)
			{
				MeshMaterial newMat;
				newMat.m_albedoOpacity = { srcMats[m].m_albedo, srcMats[m].m_opacity };
				newMat.m_metallic = srcMats[m].m_metallic;
				newMat.m_roughness = srcMats[m].m_roughness;
				newMat.m_flags = 0;
				if (srcMats[m].m_diffuseMaps.size() > 0)
				{
					newMat.m_albedoTexture = textures->LoadTexture(srcMats[m].m_diffuseMaps[0]).m_index;
				}
				if (srcMats[m].m_normalMaps.size() > 0)
				{
					newMat.m_normalTexture = textures->LoadTexture(srcMats[m].m_normalMaps[0]).m_index;
				}
				if (srcMats[m].m_roughnessMaps.size() > 0)
				{
					newMat.m_roughnessTexture = textures->LoadTexture(srcMats[m].m_roughnessMaps[0]).m_index;
				}
				if (srcMats[m].m_metalnessMaps.size() > 0)
				{
					newMat.m_metalnessTexture = textures->LoadTexture(srcMats[m].m_metalnessMaps[0]).m_index;
				}
				if (srcMats[m].m_aoMaps.size() > 0)
				{
					newMat.m_aoTexture = textures->LoadTexture(srcMats[m].m_aoMaps[0]).m_index;
				}
				m_materials.push_back(newMat);
			}
		}
	}

	void StaticMeshMaterialsComponent::Inspect(const Entities::EntityHandle& e, Entities::World* w, ValueInspector& i)
	{
		if (ImGui::Button("Populate from Static Mesh"))
		{
			PopulateFromMesh(e, w);
			i.SetModified();	// inform inspector that data changed
		}

		for (int m = 0; m < m_materials.size(); ++m)
		{
			auto inspectAlbedoOpacity = InspectComponentCustom<StaticMeshMaterialsComponent, glm::vec4>
				(e, w, [m](const Entities::EntityHandle&, StaticMeshMaterialsComponent& cmp, Entities::World*, glm::vec4 v)
			{
				if (m < cmp.m_materials.size())
				{
					cmp.m_materials[m].m_albedoOpacity = v;
				}
			});
			auto inspectAlphaPunch = InspectComponentCustom<StaticMeshMaterialsComponent, bool>
			(e, w, [m](const Entities::EntityHandle&, StaticMeshMaterialsComponent& cmp, Entities::World*, bool v)
			{
				if (m < cmp.m_materials.size())
				{
					cmp.m_materials[m].m_flags ^= (uint32_t)MeshMaterialFlags::EnablePunchThroughAlpha;
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
			auto inspectUVOffsetScale = InspectComponentCustom<StaticMeshMaterialsComponent, glm::vec4>
				(e, w, [m](const Entities::EntityHandle&, StaticMeshMaterialsComponent& cmp, Entities::World*, glm::vec4 v) {
				if (m < cmp.m_materials.size())
				{
					cmp.m_materials[m].m_uvOffsetScale = v;
				}
			});
			auto inspectAlbedoTex = InspectComponentCustom<StaticMeshMaterialsComponent, TextureHandle>
				(e, w, [m](const Entities::EntityHandle&, StaticMeshMaterialsComponent& cmp, Entities::World*, TextureHandle t)
			{
				if (m < cmp.m_materials.size())
				{
					cmp.m_materials[m].m_albedoTexture = t.m_index;
				}
			});
			auto inspectMetalTex = InspectComponentCustom<StaticMeshMaterialsComponent, TextureHandle>
				(e, w, [m](const Entities::EntityHandle&, StaticMeshMaterialsComponent& cmp, Entities::World*, TextureHandle t)
			{
				if (m < cmp.m_materials.size())
				{
					cmp.m_materials[m].m_metalnessTexture = t.m_index;
				}
			});
			auto inspectRoughnessTex = InspectComponentCustom<StaticMeshMaterialsComponent, TextureHandle>
				(e, w, [m](const Entities::EntityHandle&, StaticMeshMaterialsComponent& cmp, Entities::World*, TextureHandle t)
			{
				if (m < cmp.m_materials.size())
				{
					cmp.m_materials[m].m_roughnessTexture = t.m_index;
				}
			});
			auto inspectNormalsTex = InspectComponentCustom<StaticMeshMaterialsComponent, TextureHandle>
				(e, w, [m](const Entities::EntityHandle&, StaticMeshMaterialsComponent& cmp, Entities::World*, TextureHandle t)
			{
				if (m < cmp.m_materials.size())
				{
					cmp.m_materials[m].m_normalTexture = t.m_index;
				}
			});
			auto inspectAOTex = InspectComponentCustom<StaticMeshMaterialsComponent, TextureHandle>
				(e, w, [m](const Entities::EntityHandle&, StaticMeshMaterialsComponent& cmp, Entities::World*, TextureHandle t)
			{
				if (m < cmp.m_materials.size())
				{
					cmp.m_materials[m].m_aoTexture = t.m_index;
				}
			});
			i.InspectColour(std::format("Albedo/Opacity##{}", m).c_str(), m_materials[m].m_albedoOpacity, inspectAlbedoOpacity);
			i.Inspect(std::format("Metallic##{}", m), m_materials[m].m_metallic, inspectMetallic, 0.01f, 0.0f, 1.0f);
			i.Inspect(std::format("Roughness##{}", m), m_materials[m].m_roughness, inspectRoughness, 0.01f, 0.0f, 1.0f);
			i.Inspect(std::format("UV Offset/Scale##{}", m), m_materials[m].m_uvOffsetScale, inspectUVOffsetScale, glm::vec4(-100000.0f), glm::vec4(100000.0f));
			i.Inspect(std::format("Punch-through Alpha##{}", m), m_materials[m].m_flags & (uint32_t)MeshMaterialFlags::EnablePunchThroughAlpha, inspectAlphaPunch);
			i.InspectTexture(std::format("Albedo Texture##{}", m), TextureHandle(m_materials[m].m_albedoTexture), inspectAlbedoTex);
			i.InspectTexture(std::format("Roughness Texture##{}", m), TextureHandle(m_materials[m].m_roughnessTexture), inspectRoughnessTex);
			i.InspectTexture(std::format("Metal Texture##{}", m), TextureHandle(m_materials[m].m_metalnessTexture), inspectMetalTex);
			i.InspectTexture(std::format("Normals Texture##{}", m), TextureHandle(m_materials[m].m_normalTexture), inspectNormalsTex);
			i.InspectTexture(std::format("AO Texture##{}", m), TextureHandle(m_materials[m].m_aoTexture), inspectAOTex);
		}
	}

}