#include "static_mesh_materials.h"
#include "engine/components/static_mesh.h"
#include "engine/systems/texture_system.h"
#include "engine/systems/model_data_system.h"
#include "engine/systems/static_mesh_system.h"
#include "engine/model_data.h"
#include <imgui.h>

namespace R3
{
	void SerialiseJson(StaticMeshMaterial& smm, JsonSerialiser& s)
	{
		auto doTexture = [&](const char* name, uint32_t& i) {
			TextureHandle th = {i};
			s(name, th);
			i = th.m_index;
		};
		s("AlbedoOpacity", smm.m_albedoOpacity);
		s("Metallic", smm.m_metallic);
		s("Roughness", smm.m_roughness);
		s("ParalaxAmount", smm.m_paralaxAmount);
		s("ParalaxShadows", smm.m_paralaxShadowsEnabled);
		s("UVOffsetScale", smm.m_uvOffsetScale);
		doTexture("AlbedoTex", smm.m_albedoTexture);
		doTexture("RoughnessTex", smm.m_roughnessTexture);
		doTexture("MetalTex", smm.m_metalnessTexture);
		doTexture("NormalTex", smm.m_normalTexture);
		doTexture("AOTex", smm.m_aoTexture);
		doTexture("HeightmapTex", smm.m_heightmapTexture);
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
		if (staticMeshCmp && staticMeshCmp->m_modelHandle.m_index != -1)
		{
			auto modelData = mds->GetModelData(staticMeshCmp->m_modelHandle);
			auto& srcMats = modelData.m_data->m_materials;
			m_materials.clear();
			for (int m = 0; m < srcMats.size(); ++m)
			{
				StaticMeshMaterial newMat;
				newMat.m_albedoOpacity = { srcMats[m].m_albedo, srcMats[m].m_opacity };
				newMat.m_metallic = srcMats[m].m_metallic;
				newMat.m_roughness = srcMats[m].m_roughness;
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
				if (srcMats[m].m_heightMaps.size() > 0)
				{
					newMat.m_heightmapTexture = textures->LoadTexture(srcMats[m].m_heightMaps[0]).m_index;
				}
				m_materials.push_back(newMat);
			}
		}
		m_gpuDataIndex = -1;	// material count may have changed, reallocate gpu space for the materials
	}

	void StaticMeshMaterialsComponent::Inspect(const Entities::EntityHandle& e, Entities::World* w, ValueInspector& i)
	{
		std::string txt = std::format("GPU Data Index: {}", m_gpuDataIndex);
		ImGui::Text(txt.c_str());
		// Todo - add undo/redo friendly UI
		if (ImGui::Button("Populate from Static Mesh"))
		{
			PopulateFromMesh(e, w);
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
			auto inspectParaAmount = InspectComponentCustom<StaticMeshMaterialsComponent, float>
				(e, w, [m](const Entities::EntityHandle&, StaticMeshMaterialsComponent& cmp, Entities::World*, float v) {
				if (m < cmp.m_materials.size())
				{
					cmp.m_materials[m].m_paralaxAmount = v;
				}
			});
			auto inspectParaShadow = InspectComponentCustom<StaticMeshMaterialsComponent, bool>
				(e, w, [m](const Entities::EntityHandle&, StaticMeshMaterialsComponent& cmp, Entities::World*, bool v) {
				if (m < cmp.m_materials.size())
				{
					cmp.m_materials[m].m_paralaxShadowsEnabled = v;
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
			auto inspectHeightTex = InspectComponentCustom<StaticMeshMaterialsComponent, TextureHandle>
				(e, w, [m](const Entities::EntityHandle&, StaticMeshMaterialsComponent& cmp, Entities::World*, TextureHandle t)
			{
				if (m < cmp.m_materials.size())
				{
					cmp.m_materials[m].m_heightmapTexture = t.m_index;
				}
			});
			i.InspectColour(std::format("Albedo/Opacity##{}", m).c_str(), m_materials[m].m_albedoOpacity, inspectAlbedoOpacity);
			i.Inspect(std::format("Metallic##{}", m), m_materials[m].m_metallic, inspectMetallic, 0.01f, 0.0f, 1.0f);
			i.Inspect(std::format("Roughness##{}", m), m_materials[m].m_roughness, inspectRoughness, 0.01f, 0.0f, 1.0f);
			i.Inspect(std::format("Paralax Amount##{}", m), m_materials[m].m_paralaxAmount, inspectParaAmount, 0.01f, 0.0f, 1.0f);
			i.Inspect(std::format("Paralax Shadows##{}", m), (bool)m_materials[m].m_paralaxShadowsEnabled, inspectParaShadow);
			i.Inspect(std::format("UV Offset/Scale##{}", m), m_materials[m].m_uvOffsetScale, inspectUVOffsetScale, glm::vec4(-100000.0f), glm::vec4(100000.0f));
			i.InspectTexture(std::format("Albedo Texture##{}", m), TextureHandle(m_materials[m].m_albedoTexture), inspectAlbedoTex);
			i.InspectTexture(std::format("Roughness Texture##{}", m), TextureHandle(m_materials[m].m_roughnessTexture), inspectRoughnessTex);
			i.InspectTexture(std::format("Metal Texture##{}", m), TextureHandle(m_materials[m].m_metalnessTexture), inspectMetalTex);
			i.InspectTexture(std::format("Normals Texture##{}", m), TextureHandle(m_materials[m].m_normalTexture), inspectNormalsTex);
			i.InspectTexture(std::format("AO Texture##{}", m), TextureHandle(m_materials[m].m_aoTexture), inspectAOTex);
			i.InspectTexture(std::format("Heightmap Texture##{}", m), TextureHandle(m_materials[m].m_heightmapTexture), inspectHeightTex);
		}
	}

}