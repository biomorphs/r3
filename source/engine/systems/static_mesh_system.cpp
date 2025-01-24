#include "static_mesh_system.h"
#include "engine/systems/texture_system.h"
#include "render/render_system.h"
#include "render/device.h"
#include "render/render_pass_context.h"
#include "model_data_system.h"
#include "engine/utils/async.h"
#include "engine/ui/imgui_menubar_helper.h"
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
		m_allParts.reserve(c_maxMeshParts);
	}

	StaticMeshSystem::~StaticMeshSystem()
	{
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

	VkDeviceAddress StaticMeshSystem::GetMeshPartsDeviceAddress()
	{
		return m_allMeshPartsGpu.GetDataDeviceAddress();
	}

	VkDeviceAddress StaticMeshSystem::GetMaterialsDeviceAddress()
	{
		return m_allMaterialsGpu.GetDataDeviceAddress();
	}

	VkBuffer StaticMeshSystem::GetIndexBuffer()
	{
		return m_allIndices.GetBuffer();
	}

	bool StaticMeshSystem::GetMeshDataForModel(const ModelDataHandle& handle, MeshDrawData& result)
	{
		R3_PROF_EVENT();
		if (handle.m_index != -1)
		{
			ScopedLock lock(m_allDataMutex);
			auto foundIt = std::find_if(m_allData.begin(), m_allData.end(), [&](const MeshDrawData& d) {
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

	const MeshMaterial* StaticMeshSystem::GetMeshMaterial(uint32_t materialIndex)
	{
		assert(materialIndex < m_allMaterials.size());
		return &m_allMaterials[materialIndex];
	}

	const MeshPart* StaticMeshSystem::GetMeshPart(uint32_t partIndex)
	{
		assert(partIndex < m_allParts.size());
		return &m_allParts[partIndex];
	}

	bool StaticMeshSystem::GetMeshPart(uint32_t partIndex, MeshPart& result)
	{
		if (partIndex < m_allParts.size())
		{
			result = m_allParts[partIndex];
			return true;
		}
		return false;
	}

	bool StaticMeshSystem::GetMeshMaterial(uint32_t materialIndex, MeshMaterial& result)
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
			MeshDrawData newMesh;
			newMesh.m_vertexDataOffset = static_cast<uint32_t>(m_allVertices.Allocate(m->m_vertices.size()));
			newMesh.m_indexDataOffset = static_cast<uint32_t>(m_allIndices.Allocate(m->m_indices.size()));
			if (newMesh.m_vertexDataOffset == -1 || newMesh.m_indexDataOffset == -1)
			{
				LogError("Failed to create vertex or index buffer for mesh {}", Systems::GetSystem<ModelDataSystem>()->GetModelName(handle));
				return;
			}

			newMesh.m_materialCount = static_cast<uint32_t>(m->m_materials.size());
			newMesh.m_meshPartCount = static_cast<uint32_t>(m->m_parts.size());
			newMesh.m_totalIndices = static_cast<uint32_t>(m->m_indices.size());
			newMesh.m_totalVertices = static_cast<uint32_t>(m->m_vertices.size());
			newMesh.m_boundsMax = m->m_boundsMax;
			newMesh.m_boundsMin = m->m_boundsMin;
			newMesh.m_modelHandleIndex = handle.m_index;
			{
				ScopedLock lock(m_allDataMutex);
				uint64_t gpuIndex = m_allMaterialsGpu.Allocate(newMesh.m_materialCount);
				newMesh.m_materialGpuIndex = static_cast<uint32_t>(gpuIndex);
				for (uint32_t mat = 0; mat < newMesh.m_materialCount && gpuIndex != -1; ++mat)
				{
					auto& md = m_allMaterials[mat + gpuIndex];		// use same indices for cpu + gpu material data
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
				m_allParts.resize(m_allParts.size() + newMesh.m_meshPartCount);
				for (uint32_t part = 0; part < newMesh.m_meshPartCount; ++part)
				{
					auto& pt = m_allParts[part + newMesh.m_firstMeshPartOffset];
					pt.m_transform = m->m_parts[part].m_transform;
					pt.m_boundsMax = glm::vec4(m->m_parts[part].m_boundsMax,0);
					pt.m_boundsMin = glm::vec4(m->m_parts[part].m_boundsMin,0);
					pt.m_indexCount = m->m_parts[part].m_indexCount;
					pt.m_indexStartOffset = newMesh.m_indexDataOffset + m->m_parts[part].m_indexDataOffset;
					pt.m_materialIndex = newMesh.m_materialGpuIndex + m->m_parts[part].m_materialIndex;	// GPU index!
					pt.m_vertexDataOffset = static_cast<uint32_t>(newMesh.m_vertexDataOffset);
				}
				m_allMeshPartsGpu.Write(newMesh.m_firstMeshPartOffset, newMesh.m_meshPartCount, &m_allParts[newMesh.m_firstMeshPartOffset]);
			}
			// now copy the vertex + index data to staging
			{
				R3_PROF_EVENT("WriteGpuDataToStaging");
				m_allVertices.Write(newMesh.m_vertexDataOffset, m->m_vertices.size(), m->m_vertices.data());
				m_allIndices.Write(newMesh.m_indexDataOffset, m->m_indices.size(), m->m_indices.data());
			}
			{
				ScopedLock lock(m_allDataMutex);			// todo, m_allMaterials and allParts probably need this lock too
				m_allData.push_back(std::move(newMesh));	// push the new mesh to our array, it is ready to go!
			}
			m_onModelReadyCallbacks.Run(handle);
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
			if (!m_allVertices.Create(*ctx.m_device, c_maxVerticesToStore, c_maxVerticesToStore / 2, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT))
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
		if (!m_allMeshPartsGpu.IsCreated())
		{
			m_allMeshPartsGpu.SetDebugName("Static mesh parts");
			if (!m_allMeshPartsGpu.Create(*ctx.m_device, c_maxMeshParts, c_maxMeshParts / 8, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT))
			{
				LogError("Failed to create mesh parts buffer");
			}
			m_allMeshPartsGpu.Allocate(c_maxMeshParts);
		}
		{
			R3_PROF_EVENT("FlushStagingWrites");
			m_allVertices.Flush(*ctx.m_device, ctx.m_graphicsCmds);
			m_allIndices.Flush(*ctx.m_device, ctx.m_graphicsCmds);
			m_allMaterialsGpu.Flush(*ctx.m_device, ctx.m_graphicsCmds, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
			m_allMeshPartsGpu.Flush(*ctx.m_device, ctx.m_graphicsCmds, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT);	// mesh parts used in compute culling/bucket prep before vertex shader
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

	StaticMeshSystem::ModelReadyCallbacks::Token StaticMeshSystem::RegisterModelReadyCallback(const ModelReadyCallback& fn)
	{
		return m_onModelReadyCallbacks.AddCallback(fn);
	}

	bool StaticMeshSystem::UnregisterModelReadyCallback(ModelReadyCallbacks::Token token)
	{
		return m_onModelReadyCallbacks.RemoveCallback(token);
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
		m_onModelDataLoadedCbToken = GetSystem<ModelDataSystem>()->RegisterLoadedCallback([this](const ModelDataHandle& handle, bool loaded) {
			OnModelDataLoaded(handle, loaded);
		});
		// Cleanup must happen during render shutdown
		GetSystem<RenderSystem>()->m_onShutdownCbs.AddCallback([this](Device& d) {
			m_allMeshPartsGpu.Destroy(d);
			m_allMaterialsGpu.Destroy(d);
			m_allVertices.Destroy(d);
			m_allIndices.Destroy(d);
		});
		return true;
	}
}