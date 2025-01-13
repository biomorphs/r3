#include "static_mesh_system.h"
#include "engine/systems/texture_system.h"
#include "render/render_system.h"
#include "render/device.h"
#include "render/render_pass_context.h"
#include "model_data_system.h"
#include "engine/intersection_tests.h"
#include "engine/imgui_menubar_helper.h"
#include "engine/components/static_mesh.h"
#include "engine/components/static_mesh_materials.h"
#include "engine/components/transform.h"
#include "entities/systems/entity_system.h"
#include "entities/world.h"
#include "entities/queries.h"
#include "engine/async.h"
#include "core/log.h"
#include "core/profiler.h"
#include <imgui.h>

namespace R3
{
	StaticMeshSystem::StaticMeshSystem()
	{
		R3_PROF_EVENT();
		m_allData.reserve(1024 * 4);
		m_allMaterials.resize(c_maxMaterialsToStore);
		m_allParts.reserve(1024 * 32);
	}

	StaticMeshSystem::~StaticMeshSystem()
	{
	}

	// Searches the active world for any entities with static mesh components that intersect the ray
	// returns the closest hit entity (nearest to rayStart)
	Entities::EntityHandle StaticMeshSystem::FindClosestActiveEntityIntersectingRay(glm::vec3 rayStart, glm::vec3 rayEnd)
	{
		R3_PROF_EVENT();
		auto entities = Systems::GetSystem<Entities::EntitySystem>();
		if (entities->GetActiveWorld() == nullptr)
		{
			return {};
		}

		struct HitEntityRecord {
			Entities::EntityHandle m_entity;
			float m_hitDistance;
		};
		std::vector<HitEntityRecord> hitEntities;
		auto activeWorld = entities->GetActiveWorld();
		auto forEachEntity = [&](const Entities::EntityHandle& e, StaticMeshComponent& smc, TransformComponent& t)
		{
			if (smc.m_shouldDraw)
			{
				const auto modelData = GetSystem<ModelDataSystem>()->GetModelData(smc.m_modelHandle);
				if (modelData.m_data)
				{
					// transform the ray into model space so we can do a simple AABB test
					const glm::mat4 inverseTransform = glm::inverse(t.GetWorldspaceMatrix(e, *activeWorld));
					const auto rs = glm::vec3(inverseTransform * glm::vec4(rayStart, 1));
					const auto re = glm::vec3(inverseTransform * glm::vec4(rayEnd, 1));
					float hitT = 0.0f;
					if (RayIntersectsAABB(rs, re, modelData.m_data->m_boundsMin, modelData.m_data->m_boundsMax, hitT))
					{
						hitEntities.push_back({ e, hitT });
					}
				}
			}
			return true;
		};
		Entities::Queries::ForEach<StaticMeshComponent, TransformComponent>(activeWorld, forEachEntity);

		// now find the closest hit entity that is in front of the ray
		Entities::EntityHandle closestHit = {};
		float closestHitDistance = FLT_MAX;
		for (int i = 0; i < hitEntities.size(); ++i)
		{
			if (hitEntities[i].m_hitDistance >= 0.0f && hitEntities[i].m_hitDistance < closestHitDistance)
			{
				closestHit = hitEntities[i].m_entity;
				closestHitDistance = hitEntities[i].m_hitDistance;
			}
		}
		return closestHit;
	}

	void StaticMeshSystem::RegisterTickFns()
	{
		R3_PROF_EVENT();
		RegisterTick("StaticMeshes::ShowGui", [this]() {
			return ShowGui();
		});
	}

	void StaticMeshSystem::Shutdown()
	{
		R3_PROF_EVENT();
		Systems::GetSystem<ModelDataSystem>()->UnregisterLoadedCallback(m_onModelDataLoadedCbToken);
	}

	VkDeviceAddress StaticMeshSystem::GetVertexDataDeviceAddress()
	{
		return m_allVertices.GetDataDeviceAddress();
	}

	VkDeviceAddress StaticMeshSystem::GetMaterialsDeviceAddress()
	{
		return m_allMaterialsGpu.GetDataDeviceAddress();
	}

	VkBuffer StaticMeshSystem::GetIndexBuffer()
	{
		return m_allIndices.GetBuffer();
	}

	bool StaticMeshSystem::GetMeshDataForModel(const ModelDataHandle& handle, StaticMeshGpuData& result)
	{
		R3_PROF_EVENT();
		if (handle.m_index != -1)
		{
			ScopedLock lock(m_allDataMutex);
			auto foundIt = std::find_if(m_allData.begin(), m_allData.end(), [&](const StaticMeshGpuData& d) {
				return d.m_modelHandleIndex == handle.m_index;
			});
			if (foundIt != m_allData.end())
			{
				result = *foundIt;
				return true;
			}
		}
		return false;
	}

	const StaticMeshMaterial* StaticMeshSystem::GetMeshMaterial(uint32_t materialIndex)
	{
		assert(materialIndex < m_allMaterials.size());
		return &m_allMaterials[materialIndex];
	}

	const StaticMeshPart* StaticMeshSystem::GetMeshPart(uint32_t partIndex)
	{
		assert(partIndex < m_allParts.size());
		return &m_allParts[partIndex];
	}

	bool StaticMeshSystem::GetMeshPart(uint32_t partIndex, StaticMeshPart& result)
	{
		if (partIndex < m_allParts.size())
		{
			result = m_allParts[partIndex];
			return true;
		}
		return false;
	}

	bool StaticMeshSystem::GetMeshMaterial(uint32_t materialIndex, StaticMeshMaterial& result)
	{
		if (materialIndex < m_allMaterials.size())
		{
			result = m_allMaterials[materialIndex];
			return true;
		}
		return false;
	}

	bool StaticMeshSystem::PrepareForUpload(const ModelDataHandle& handle)
	{
		R3_PROF_EVENT();
		auto prepMeshData = [this, handle]() {
			R3_PROF_EVENT("PrepareStaticMeshForUpload");
			auto mdata = Systems::GetSystem<ModelDataSystem>()->GetModelData(handle);
			auto textures = Systems::GetSystem<TextureSystem>();
			auto& m = mdata.m_data;

			// allocate the mesh data + fill in what we can while we have the lock
			StaticMeshGpuData newMesh;
			newMesh.m_vertexDataOffset = m_allVertices.Allocate(m->m_vertices.size());
			newMesh.m_indexDataOffset = m_allIndices.Allocate(m->m_indices.size());
			if (newMesh.m_vertexDataOffset == -1 || newMesh.m_indexDataOffset == -1)
			{
				LogError("Failed to create vertex or index buffer for mesh {}", Systems::GetSystem<ModelDataSystem>()->GetModelName(handle));
				return;
			}

			newMesh.m_materialCount = static_cast<uint32_t>(m->m_materials.size());
			newMesh.m_meshPartCount = static_cast<uint32_t>(m->m_meshes.size());
			newMesh.m_totalIndices = static_cast<uint32_t>(m->m_indices.size());
			newMesh.m_totalVertices = static_cast<uint32_t>(m->m_vertices.size());
			newMesh.m_boundsMax = m->m_boundsMax;
			newMesh.m_boundsMin = m->m_boundsMin;
			newMesh.m_modelHandleIndex = handle.m_index;
			{
				ScopedLock lock(m_allDataMutex);
				uint64_t gpuIndex = m_allMaterialsGpu.Allocate(newMesh.m_materialCount);
				newMesh.m_materialGpuIndex = static_cast<uint32_t>(gpuIndex);
				for (uint32_t mat = 0; mat < newMesh.m_materialCount; ++mat)	// write cpu materials offset from gpuIndex
				{
					auto& md = m_allMaterials[mat + gpuIndex];
					md.m_albedoOpacity = { m->m_materials[mat].m_albedo, m->m_materials[mat].m_opacity };
					md.m_metallic = m->m_materials[mat].m_metallic;
					md.m_roughness = m->m_materials[mat].m_roughness;
					if (m->m_materials[mat].m_diffuseMaps.size() > 0)
					{
						md.m_albedoTexture = textures->LoadTexture(m->m_materials[mat].m_diffuseMaps[0]).m_index;
					}
					if (m->m_materials[mat].m_normalMaps.size() > 0)
					{
						md.m_normalTexture = textures->LoadTexture(m->m_materials[mat].m_normalMaps[0]).m_index;
					}
					if (m->m_materials[mat].m_roughnessMaps.size() > 0)
					{
						md.m_roughnessTexture = textures->LoadTexture(m->m_materials[mat].m_roughnessMaps[0]).m_index;
					}
					if (m->m_materials[mat].m_metalnessMaps.size() > 0)
					{
						md.m_metalnessTexture = textures->LoadTexture(m->m_materials[mat].m_metalnessMaps[0]).m_index;
					}
					if (m->m_materials[mat].m_aoMaps.size() > 0)
					{
						md.m_aoTexture = textures->LoadTexture(m->m_materials[mat].m_aoMaps[0]).m_index;
					}
				}
				if (gpuIndex != -1)
				{
					m_allMaterialsGpu.Write(gpuIndex, newMesh.m_materialCount, &m_allMaterials[gpuIndex]);
				}
				
				newMesh.m_firstMeshPartOffset = static_cast<uint32_t>(m_allParts.size());
				m_allParts.resize(m_allParts.size() + m->m_meshes.size());
				for (uint32_t part = 0; part < newMesh.m_meshPartCount; ++part)
				{
					auto& pt = m_allParts[part + newMesh.m_firstMeshPartOffset];
					pt.m_transform = m->m_meshes[part].m_transform;
					pt.m_boundsMax = m->m_meshes[part].m_boundsMax;
					pt.m_boundsMin = m->m_meshes[part].m_boundsMin;
					pt.m_indexCount = m->m_meshes[part].m_indexCount;
					pt.m_indexStartOffset = newMesh.m_indexDataOffset + m->m_meshes[part].m_indexDataOffset;
					pt.m_materialIndex = newMesh.m_materialGpuIndex + m->m_meshes[part].m_materialIndex;	// GPU index!
					pt.m_vertexDataOffset = static_cast<uint32_t>(newMesh.m_vertexDataOffset);
				}
			}
			// now copy the vertex + index data to staging
			{
				R3_PROF_EVENT("WriteGpuDataToStaging");
				m_allVertices.Write(newMesh.m_vertexDataOffset, m->m_vertices.size(), m->m_vertices.data());
				m_allIndices.Write(newMesh.m_indexDataOffset, m->m_indices.size(), m->m_indices.data());
			}
			{
				ScopedLock lock(m_allDataMutex);
				m_allData.push_back(std::move(newMesh));	// push the new mesh to our array, it is ready to go!
			}
		};
		RunAsync(std::move(prepMeshData));
		return true;
	}

	void StaticMeshSystem::PrepareForRendering(class RenderPassContext& ctx)
	{
		R3_PROF_EVENT();
		if (!m_allVertices.IsCreated())
		{
			m_allVertices.SetDebugName("Static mesh vertices");
			if (!m_allVertices.Create(*ctx.m_device, c_maxVerticesToStore, c_maxVerticesToStore, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT))
			{
				LogError("Failed to create vertex buffer");
			}
		}
		if (!m_allIndices.IsCreated())
		{
			m_allIndices.SetDebugName("Static mesh indices");
			if (!m_allIndices.Create(*ctx.m_device, c_maxIndicesToStore, c_maxIndicesToStore / 2, VK_BUFFER_USAGE_INDEX_BUFFER_BIT))
			{
				LogError("Failed to create index buffer");
			}
		}
		if (!m_allMaterialsGpu.IsCreated())
		{
			m_allMaterialsGpu.SetDebugName("Static mesh materials");
			if (!m_allMaterialsGpu.Create(*ctx.m_device, c_maxMaterialsToStore, c_maxMaterialsToStore, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT))
			{
				LogError("Failed to create material buffer");
			}
		}
		{
			R3_PROF_EVENT("UploadInstanceMaterials");
			auto entities = Systems::GetSystem<Entities::EntitySystem>();
			auto forEachEntity = [&](const Entities::EntityHandle& e, StaticMeshMaterialsComponent& smc)
				{
					if (smc.m_gpuDataIndex == -1 && smc.m_materials.size() > 0)
					{
						smc.m_gpuDataIndex = static_cast<uint32_t>(m_allMaterialsGpu.Allocate(smc.m_materials.size()));
					}
					if (smc.m_gpuDataIndex != -1)	// update all instance mats each frame
					{
						m_allMaterialsGpu.Write(smc.m_gpuDataIndex, smc.m_materials.size(), smc.m_materials.data());
						memcpy(&m_allMaterials[smc.m_gpuDataIndex], smc.m_materials.data(), smc.m_materials.size() * sizeof(StaticMeshMaterial));
					}
					return true;
				};

			if (entities->GetActiveWorld())
			{
				Entities::Queries::ForEach<StaticMeshMaterialsComponent>(entities->GetActiveWorld(), forEachEntity);
			}
		}
		{
			R3_PROF_EVENT("FlushStagingWrites");
			m_allVertices.Flush(*ctx.m_device, ctx.m_graphicsCmds);
			m_allIndices.Flush(*ctx.m_device, ctx.m_graphicsCmds);
			m_allMaterialsGpu.Flush(*ctx.m_device, ctx.m_graphicsCmds);
		}
	}

	bool StaticMeshSystem::ShowGui()
	{
		R3_PROF_EVENT();
		auto& debugMenu = MenuBar::MainMenu().GetSubmenu("Debug");
		debugMenu.AddItem("Static Meshes", [this]() {
			m_showGui = !m_showGui;
		});
		if (m_showGui)
		{
			std::string txt;
			ImGui::Begin("Static Meshes");
			{
				ScopedTryLock lock(m_allDataMutex);
				if (lock.IsLocked())
				{
					txt = std::format("{} static models uploaded", m_allData.size());
					ImGui::Text(txt.c_str());
					txt = std::format("{} materials", m_allMaterials.size());
					ImGui::Text(txt.c_str());
					txt = std::format("{} parts", m_allParts.size());
					ImGui::Text(txt.c_str());
					txt = std::format("{} / {} vertices", m_allVertices.GetAllocated(), m_allVertices.GetMaxAllocated());
					ImGui::Text(txt.c_str());
					ImGui::SameLine();
					ImGui::ProgressBar((float)m_allVertices.GetAllocated() / (float)m_allVertices.GetMaxAllocated());
					txt = std::format("{} / {} indices", m_allIndices.GetAllocated(), m_allIndices.GetMaxAllocated());
					ImGui::Text(txt.c_str());
					ImGui::SameLine();
					ImGui::ProgressBar((float)m_allIndices.GetAllocated() / (float)m_allIndices.GetMaxAllocated());
				}
			}
			ImGui::End();
		}
		return true;
	}

	void StaticMeshSystem::OnModelDataLoaded(const ModelDataHandle& handle, bool loaded)
	{
		R3_PROF_EVENT();
		auto models = Systems::GetSystem<ModelDataSystem>();
		if (!loaded)
		{
			LogError("Failed to load model data '{}'", models->GetModelName(handle));
		}
		else
		{
			if (!PrepareForUpload(handle))
			{
				LogWarn("Failed to prepare model {} for upload", models->GetModelName(handle));
			}
		}
	}

	bool StaticMeshSystem::Init()
	{
		R3_PROF_EVENT();
		// Register as a listener for any new loaded models
		auto models = Systems::GetSystem<ModelDataSystem>();
		m_onModelDataLoadedCbToken = models->RegisterLoadedCallback([this](const ModelDataHandle& handle, bool loaded) {
			OnModelDataLoaded(handle, loaded);
		});
		// Register render functions
		auto render = Systems::GetSystem<RenderSystem>();
		render->m_onShutdownCbs.AddCallback([this](Device& d) {
			m_allMaterialsGpu.Destroy(d);
			m_allVertices.Destroy(d);
			m_allIndices.Destroy(d);
		});
		return true;
	}
}