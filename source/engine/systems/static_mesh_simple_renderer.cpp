#include "static_mesh_simple_renderer.h"
#include "static_mesh_system.h"
#include "camera_system.h"
#include "lights_system.h"
#include "texture_system.h"
#include "time_system.h"
#include "engine/imgui_menubar_helper.h"
#include "engine/components/transform.h"
#include "engine/components/static_mesh.h"
#include "engine/components/static_mesh_materials.h"
#include "engine/components/environment_settings.h"
#include "entities/systems/entity_system.h"
#include "entities/world.h"
#include "entities/queries.h"
#include "render/render_system.h"
#include "render/device.h"
#include "render/pipeline_builder.h"
#include "render/descriptors.h"
#include "render/render_pass_context.h"
#include "render/render_target_cache.h"
#include "core/profiler.h"
#include "core/log.h"
#include <imgui.h>

namespace R3
{
	// stored in a buffer
	struct StaticMeshSimpleRenderer::GlobalConstants
	{
		glm::mat4 m_projViewTransform;
		glm::vec4 m_cameraWorldSpacePos;
		glm::vec4 m_sunColourAmbient = {0,0,0,0};
		glm::vec4 m_sunDirectionBrightness = { 0,-1,0,0 };
		glm::vec4 m_skyColourAmbient = { 0,0,0,0 };
		VkDeviceAddress m_vertexBufferAddress;
		VkDeviceAddress m_materialBufferAddress;
		VkDeviceAddress m_pointLightsBufferAddress;
		uint32_t m_firstPointLightOffset;
		uint32_t m_pointlightCount;
	};

	// one per draw call
	struct PushConstants 
	{
		uint32_t m_globalConstantIndex;
		uint32_t m_padding;
	};

	StaticMeshSimpleRenderer::StaticMeshSimpleRenderer()
	{
	}

	StaticMeshSimpleRenderer::~StaticMeshSimpleRenderer()
	{
	}

	void StaticMeshSimpleRenderer::RegisterTickFns()
	{
		R3_PROF_EVENT();
		RegisterTick("StaticMeshSimpleRenderer::ShowGui", [this]() {
			return ShowGui();
		});
		RegisterTick("StaticMeshSimpleRenderer::CollectInstances", [this]() {
			return CollectInstances();
		});
	}

	bool StaticMeshSimpleRenderer::Init()
	{
		R3_PROF_EVENT();
		auto render = Systems::GetSystem<RenderSystem>();
		render->m_onShutdownCbs.AddCallback([this](Device& d) {
			Cleanup(d);
		});
		return true;
	}

	void StaticMeshSimpleRenderer::Cleanup(Device& d)
	{
		R3_PROF_EVENT();
		vkDestroyPipeline(d.GetVkDevice(), m_simpleTriPipeline, nullptr);
		vkDestroyPipelineLayout(d.GetVkDevice(), m_pipelineLayout, nullptr);
		if (m_globalInstancesHostVisible.m_allocation)
		{
			vmaUnmapMemory(d.GetVMA(), m_globalInstancesHostVisible.m_allocation);
			vmaDestroyBuffer(d.GetVMA(), m_globalInstancesHostVisible.m_buffer, m_globalInstancesHostVisible.m_allocation);
		}
		if (m_drawIndirectHostVisible.m_allocation)
		{
			vmaUnmapMemory(d.GetVMA(), m_drawIndirectHostVisible.m_allocation);
			vmaDestroyBuffer(d.GetVMA(), m_drawIndirectHostVisible.m_buffer, m_drawIndirectHostVisible.m_allocation);
		}
		vkDestroyDescriptorSetLayout(d.GetVkDevice(), m_globalsDescriptorLayout, nullptr);
		m_descriptorAllocator = {};
		m_globalConstantsBuffer.Destroy(d);
	}

	bool StaticMeshSimpleRenderer::ShowGui()
	{
		R3_PROF_EVENT();
		auto& debugMenu = MenuBar::MainMenu().GetSubmenu("Debug");
		debugMenu.AddItem("Static Mesh Renderer", [this]() {
			m_showGui = !m_showGui;
		});
		if (m_showGui)
		{
			ImGui::Begin("Static Mesh Renderer");
			std::string txt = std::format("{} Model Instances Submitted", m_frameStats.m_totalModelInstances);
			ImGui::Text(txt.c_str());
			txt = std::format("    {} Part Instances", m_frameStats.m_totalPartInstances);
			ImGui::Text(txt.c_str());
			txt = std::format("    {:L} Triangles", m_frameStats.m_totalTriangles);
			ImGui::Text(txt.c_str());
			txt = std::format("Command buffer took {:.3f}ms to build", 1000.0 * (m_frameStats.m_writeCmdsEndTime - m_frameStats.m_writeCmdsStartTime));
			ImGui::Text(txt.c_str());
			ImGui::End();
		}
		return true;
	}

	bool StaticMeshSimpleRenderer::CreatePipelineData(Device& d, VkFormat mainColourFormat, VkFormat mainDepthFormat)
	{
		R3_PROF_EVENT();
		auto textures = GetSystem<TextureSystem>();
		VkPushConstantRange constantRange;	// Create pipeline layout
		constantRange.offset = 0;	// needs to match in the shader if >0!
		constantRange.size = sizeof(PushConstants);
		constantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
		VkPipelineLayoutCreateInfo pipelineLayoutInfo = { 0 };
		pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutInfo.pushConstantRangeCount = 1;
		pipelineLayoutInfo.pPushConstantRanges = &constantRange;
		VkDescriptorSetLayout setLayouts[] = { m_globalsDescriptorLayout, textures->GetDescriptorsLayout() };
		pipelineLayoutInfo.pSetLayouts = setLayouts;
		pipelineLayoutInfo.setLayoutCount = 2;
		if (!VulkanHelpers::CheckResult(vkCreatePipelineLayout(d.GetVkDevice(), &pipelineLayoutInfo, nullptr, &m_pipelineLayout)))
		{
			LogError("Failed to create pipeline layout");
			return false;
		}
		std::string basePath = "shaders_spirv\\common\\";	// Load the shaders
		auto vertexShader = VulkanHelpers::LoadShaderModule(d.GetVkDevice(), basePath + "static_mesh_simple_main_pass.vert.spv");
		auto fragShader = VulkanHelpers::LoadShaderModule(d.GetVkDevice(), basePath + "static_mesh_simple_main_pass.frag.spv");
		if (vertexShader == VK_NULL_HANDLE || fragShader == VK_NULL_HANDLE)
		{
			LogError("Failed to create shader modules");
			return false;
		}
		PipelineBuilder pb;	// Make the pipeline
		pb.m_shaderStages.push_back(VulkanHelpers::CreatePipelineShaderState(VK_SHADER_STAGE_VERTEX_BIT, vertexShader));
		pb.m_shaderStages.push_back(VulkanHelpers::CreatePipelineShaderState(VK_SHADER_STAGE_FRAGMENT_BIT, fragShader));
		std::vector<VkDynamicState> dynamicStates = {
			VK_DYNAMIC_STATE_VIEWPORT,
			VK_DYNAMIC_STATE_SCISSOR
		};
		pb.m_dynamicState = VulkanHelpers::CreatePipelineDynamicState(dynamicStates);
		pb.m_vertexInputState = VulkanHelpers::CreatePipelineEmptyVertexInputState();
		pb.m_inputAssemblyState = VulkanHelpers::CreatePipelineInputAssemblyState(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
		pb.m_viewportState = VulkanHelpers::CreatePipelineDynamicViewportState();
		pb.m_rasterState = VulkanHelpers::CreatePipelineRasterState(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE);
		pb.m_multisamplingState = VulkanHelpers::CreatePipelineMultiSampleState_SingleSample();
		VkPipelineDepthStencilStateCreateInfo depthStencilState = { 0 };	// Enable depth read/write
		depthStencilState.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		depthStencilState.depthTestEnable = VK_TRUE;
		depthStencilState.depthWriteEnable = VK_TRUE;
		depthStencilState.depthCompareOp = VK_COMPARE_OP_LESS;
		depthStencilState.depthBoundsTestEnable = VK_FALSE;
		depthStencilState.stencilTestEnable = VK_FALSE;
		pb.m_depthStencilState = depthStencilState;
		std::vector<VkPipelineColorBlendAttachmentState> allAttachments = {	// No colour attachment blending
			VulkanHelpers::CreatePipelineColourBlendAttachment_NoBlending()
		};
		pb.m_colourBlendState = VulkanHelpers::CreatePipelineColourBlendState(allAttachments);
		m_simpleTriPipeline = pb.Build(d.GetVkDevice(), m_pipelineLayout, 1, &mainColourFormat, mainDepthFormat);
		if (m_simpleTriPipeline == VK_NULL_HANDLE)
		{
			LogError("Failed to create pipeline!");
			return false;
		}
		vkDestroyShaderModule(d.GetVkDevice(), vertexShader, nullptr);
		vkDestroyShaderModule(d.GetVkDevice(), fragShader, nullptr);
		return true;
	}

	bool StaticMeshSimpleRenderer::CreateGlobalDescriptorSet()
	{
		R3_PROF_EVENT();
		auto render = GetSystem<RenderSystem>();
		DescriptorLayoutBuilder layoutBuilder;
		layoutBuilder.AddBinding(0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);		// uniform buffer for global constants
		layoutBuilder.AddBinding(1, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);		// storage buffer for instance data
		m_globalsDescriptorLayout = layoutBuilder.Create(*render->GetDevice(), false);	// dont need bindless here
		if (m_globalsDescriptorLayout == nullptr)
		{
			LogError("Failed to create global descriptor set layout");
			return false;
		}

		m_descriptorAllocator = std::make_unique<DescriptorSetSimpleAllocator>();
		std::vector<VkDescriptorPoolSize> poolSizes = {
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1 }
		};
		if (!m_descriptorAllocator->Initialise(*render->GetDevice(), 1, poolSizes))
		{
			LogError("Failed to create descriptor allocator");
			return false;
		}

		// Create the global set and write it now, it will never be updated again
		m_globalDescriptorSet = m_descriptorAllocator->Allocate(*render->GetDevice(), m_globalsDescriptorLayout);
		DescriptorSetWriter writer(m_globalDescriptorSet);
		writer.WriteUniformBuffer(0, m_globalConstantsBuffer.GetBuffer());
		writer.WriteStorageBuffer(1, m_globalInstancesHostVisible.m_buffer);
		writer.FlushWrites();

		return true;
	}

	bool StaticMeshSimpleRenderer::InitialiseGpuData(Device& d, VkFormat mainColourFormat, VkFormat mainDepthFormat)
	{
		if (!m_globalConstantsBuffer.IsCreated())
		{
			m_globalConstantsBuffer.SetDebugName("Static mesh global constants");
			if (!m_globalConstantsBuffer.Create(d, c_maxGlobalConstantBuffers, c_maxGlobalConstantBuffers, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT))
			{
				LogError("Failed to create constant buffer");
				return false;
			}
			m_globalConstantsBuffer.Allocate(c_maxGlobalConstantBuffers);
		}
		if (m_globalInstancesMappedPtr == nullptr)
		{
			m_globalInstancesHostVisible = VulkanHelpers::CreateBuffer(d.GetVMA(),
				c_maxInstances * c_maxInstanceBuffers * sizeof(StaticMeshInstanceGpu),
				VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_AUTO, VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);
			VulkanHelpers::SetBufferName(d.GetVkDevice(), m_globalInstancesHostVisible, "Static mesh global instances");
			void* mapped = nullptr;
			vmaMapMemory(d.GetVMA(), m_globalInstancesHostVisible.m_allocation, &mapped);
			m_globalInstancesMappedPtr = static_cast<StaticMeshInstanceGpu*>(mapped);
		}
		if (m_drawIndirectMappedPtr == nullptr)
		{
			m_drawIndirectHostVisible = VulkanHelpers::CreateBuffer(d.GetVMA(),
				c_maxInstances * c_maxInstanceBuffers * sizeof(VkDrawIndexedIndirectCommand),
				VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT, VMA_MEMORY_USAGE_AUTO, VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);
			VulkanHelpers::SetBufferName(d.GetVkDevice(), m_drawIndirectHostVisible, "Static mesh draw indirect");
			void* mapped = nullptr;
			vmaMapMemory(d.GetVMA(), m_drawIndirectHostVisible.m_allocation, &mapped);
			m_drawIndirectMappedPtr = static_cast<StaticMeshInstanceGpu*>(mapped);
		}
		if (m_globalDescriptorSet == VK_NULL_HANDLE)
		{
			if (!CreateGlobalDescriptorSet())
			{
				LogError("Failed to create global descriptor set");
				return false;
			}
		}
		if (m_pipelineLayout == VK_NULL_HANDLE || m_simpleTriPipeline == VK_NULL_HANDLE)
		{
			CreatePipelineData(d, mainColourFormat, mainDepthFormat);
		}
		return true;
	}

	void StaticMeshSimpleRenderer::ProcessEnvironmentSettings(GlobalConstants& g)
	{
		R3_PROF_EVENT();
		auto entities = GetSystem<Entities::EntitySystem>();
		auto w = entities->GetActiveWorld();
		if (w)
		{
			auto getSettings = [&g, this](const Entities::EntityHandle& e, EnvironmentSettingsComponent& cmp) {
				g.m_sunColourAmbient = { cmp.m_sunColour, cmp.m_sunAmbientFactor };
				g.m_skyColourAmbient = { cmp.m_skyColour, cmp.m_skyAmbientFactor };
				g.m_sunDirectionBrightness = { glm::normalize(cmp.m_sunDirection), cmp.m_sunBrightness };
				m_mainPassColourClearValue = glm::vec4(cmp.m_skyColour, 1);
				return true;
			};
			Entities::Queries::ForEach<EnvironmentSettingsComponent>(w, getSettings);
		}
	}

	/////////////////////////////////////////////////////////////////
	// new stuff
	void StaticMeshSimpleRenderer::CollectModelInstancesFromEntities()
	{
		R3_PROF_EVENT();
		for (auto& instanceList : m_allMeshInstances)
		{
			instanceList.second.clear();	// just empty the array for each mesh to avoid constant reallocations
		}
		auto activeWorld = Systems::GetSystem<Entities::EntitySystem>()->GetActiveWorld();
		if (activeWorld)
		{
			// cache previous instance array to avoid lookups for consecutive entities with same model
			uint32_t lastMeshHandle = -1;
			std::vector<StaticMeshInstance>* lastMeshInstanceArray = nullptr;
			auto forEachEntity = [&](const Entities::EntityHandle& e, StaticMeshComponent& s, TransformComponent& t)
			{
				if (s.m_modelHandle.m_index != -1 && s.m_shouldDraw)	// doesn't mean the model is actually ready to draw!
				{
					StaticMeshInstance meshInstance;
					if (s.m_materialOverride.GetID() != -1)
					{
						if (auto matOverrideCmp = activeWorld->GetComponent<StaticMeshMaterialsComponent>(s.m_materialOverride))
						{
							meshInstance.m_materialBaseIndex = matOverrideCmp->m_gpuDataIndex;	// not validated yet!
						}
					}
					meshInstance.m_transform = t.GetWorldspaceInterpolated(e, *activeWorld);

					if (lastMeshHandle != s.m_modelHandle.m_index)
					{
						lastMeshInstanceArray = &m_allMeshInstances[s.m_modelHandle.m_index];
						lastMeshHandle = s.m_modelHandle.m_index;
					}
					lastMeshInstanceArray->push_back(meshInstance);
				}
				return true;
			};
			Entities::Queries::ForEach<StaticMeshComponent, TransformComponent>(activeWorld, forEachEntity);
		}
	}

	void StaticMeshSimpleRenderer::CollectModelPartInstances()
	{
		R3_PROF_EVENT();
		auto staticMeshes = GetSystem<StaticMeshSystem>();
		m_allPartInstances.clear();
		m_opaqueInstances.clear();

		for (const auto &meshEntry : m_allMeshInstances)	// for each model
		{
			StaticMeshGpuData currentMeshData;
			if (meshEntry.second.size() > 0 && staticMeshes->GetMeshDataForModel(ModelDataHandle(meshEntry.first), currentMeshData))
			{
				for (uint32_t partIndex = 0; partIndex < currentMeshData.m_meshPartCount; ++partIndex)	// for each part
				{
					StaticMeshPart partData;
					if (staticMeshes->GetMeshPart(currentMeshData.m_firstMeshPartOffset + partIndex, partData))
					{
						for (const auto& meshInstance : meshEntry.second)	// for each instance of this part
						{						
							StaticMeshPartInstance partInstance;
							partInstance.m_fullTransform = meshInstance.m_transform * partData.m_transform;	// final part transform
							partInstance.m_resolvedMaterialIndex = meshInstance.m_materialBaseIndex == -1 ? currentMeshData.m_materialGpuIndex : meshInstance.m_materialBaseIndex;	// base index
							partInstance.m_resolvedMaterialIndex += partData.m_materialIndex - currentMeshData.m_materialGpuIndex;	// relative to part materials 
							partInstance.m_partGlobalIndex = currentMeshData.m_firstMeshPartOffset + partIndex;
							m_allPartInstances.emplace_back(partInstance);			// add resolved part data to global array
							const auto globalPartIndex = static_cast<uint32_t>(m_allPartInstances.size() - 1);

							// insert part instances into draw buckets depending on material, flags, etc
							m_opaqueInstances.push_back(globalPartIndex);
						}
					}
				}
			}
		}
	}

	void StaticMeshSimpleRenderer::PrepareOpaqueDrawData()
	{
		R3_PROF_EVENT();
		auto staticMeshes = GetSystem<StaticMeshSystem>();
		uint32_t thisDrawOffset = m_currentDrawBufferStart + m_currentFrameTotalDraws;
		uint32_t thisInstanceOffset = m_currentInstanceBufferStart + m_currentFrameTotalInstances;
		m_opaqueDrawsStart = thisDrawOffset;
		StaticMeshPart partData;
		uint32_t lastPartDataIndex = -1;
		for (const auto& partInstanceIndex : m_opaqueInstances)		// indexes into m_allPartInstances
		{
			const auto& partInstanceData = m_allPartInstances[partInstanceIndex];
			if (lastPartDataIndex != partInstanceData.m_partGlobalIndex)
			{
				staticMeshes->GetMeshPart(partInstanceData.m_partGlobalIndex, partData);
				lastPartDataIndex = partInstanceData.m_partGlobalIndex;
			}
			
			// We can do culling here

			// Write the instance data
			StaticMeshInstanceGpu* instancesPtr = static_cast<StaticMeshInstanceGpu*>(m_globalInstancesMappedPtr) + thisInstanceOffset;
			instancesPtr->m_transform = partInstanceData.m_fullTransform;
			instancesPtr->m_materialIndex = partInstanceData.m_resolvedMaterialIndex;

			// write a draw call
			VkDrawIndexedIndirectCommand* drawPtr = static_cast<VkDrawIndexedIndirectCommand*>(m_drawIndirectMappedPtr) + thisDrawOffset;
			drawPtr->firstInstance = thisInstanceOffset;
			drawPtr->instanceCount = 1;
			drawPtr->firstIndex = static_cast<uint32_t>(partData.m_indexStartOffset);
			drawPtr->indexCount = partData.m_indexCount;
			drawPtr->vertexOffset = partData.m_vertexDataOffset;

			thisInstanceOffset++;
			thisDrawOffset++;
		}
		const auto drawCount = thisDrawOffset - (m_currentDrawBufferStart + m_currentFrameTotalDraws);
		const auto instanceCount = thisInstanceOffset - (m_currentInstanceBufferStart + m_currentFrameTotalInstances);
		m_currentFrameTotalDraws += drawCount;
		m_currentFrameTotalInstances += instanceCount;
		m_opaqueDrawsCount = drawCount;
	}

	/////////////////////////////////////////////////////////////////

	uint32_t StaticMeshSimpleRenderer::WriteInstances()
	{
		R3_PROF_EVENT();
		
		auto staticMeshes = GetSystem<StaticMeshSystem>();
		auto activeWorld = Systems::GetSystem<Entities::EntitySystem>()->GetActiveWorld();
		if (activeWorld == nullptr)
		{
			return 0;
		}
		StaticMeshGpuData currentMeshData;				// Try to cache what we can while iterating, yes this is messy, but its fast!
		ModelDataHandle currentMeshDataHandle;
		std::vector<StaticMeshPart> currentMeshParts;	// cache the parts to avoid touching the meshes constantly
		uint32_t lastMatOverrideGpuIndex = -1;			// index into gpu materials from the last material component
		Entities::EntityHandle lastMatEntity;
		uint32_t thisInstanceOffset = m_currentInstanceBufferStart;
		auto forEach = [&](const Entities::EntityHandle& e, StaticMeshComponent& s, TransformComponent& t)
		{
			if (s.m_modelHandle.m_index == -1 || !s.m_shouldDraw)
			{
				return true;
			}
			if (s.m_modelHandle.m_index != currentMeshDataHandle.m_index)
			{
				if (staticMeshes->GetMeshDataForModel(s.m_modelHandle, currentMeshData))
				{
					currentMeshParts.resize(currentMeshData.m_meshPartCount);
					currentMeshDataHandle = s.m_modelHandle;
					for (uint32_t p = 0; p < currentMeshData.m_meshPartCount; ++p)
					{
						staticMeshes->GetMeshPart(currentMeshData.m_firstMeshPartOffset + p, currentMeshParts[p]);
					}
				}
				else
				{
					currentMeshData = {};
					currentMeshParts.clear();
					return true;
				}
			}
			if (currentMeshDataHandle.m_index == s.m_modelHandle.m_index)
			{
				if (s.m_materialOverride != lastMatEntity)
				{
					auto lastMatCmp = activeWorld->GetComponent<StaticMeshMaterialsComponent>(s.m_materialOverride);
					lastMatOverrideGpuIndex = lastMatCmp ? static_cast<uint32_t>(lastMatCmp->m_gpuDataIndex) : -1;
					lastMatEntity = s.m_materialOverride;
				}
				const uint32_t meshMaterialIndex = currentMeshData.m_materialGpuIndex;
				const uint32_t meshVertexDataOffset = (uint32_t)currentMeshData.m_vertexDataOffset;
				const glm::mat4 compTransform = t.GetWorldspaceInterpolated(e, *activeWorld);
				const bool useOverrides = lastMatOverrideGpuIndex != -1 && lastMatOverrideGpuIndex >= currentMeshData.m_materialCount;
				for (uint32_t part = 0; part < currentMeshParts.size(); ++part)
				{
					auto relativePartMatIndex = currentMeshParts[part].m_materialIndex - meshMaterialIndex;
					StaticMeshInstanceGpu* instancePtr = static_cast<StaticMeshInstanceGpu*>(m_globalInstancesMappedPtr) + thisInstanceOffset;
					instancePtr->m_materialIndex = useOverrides ? ((uint32_t)lastMatOverrideGpuIndex + relativePartMatIndex) : currentMeshParts[part].m_materialIndex;
					instancePtr->m_transform = compTransform * currentMeshParts[part].m_transform;
					VkDrawIndexedIndirectCommand* drawPtr = static_cast<VkDrawIndexedIndirectCommand*>(m_drawIndirectMappedPtr) + thisInstanceOffset;
					drawPtr->indexCount = currentMeshParts[part].m_indexCount;
					drawPtr->instanceCount = 1;
					drawPtr->firstIndex = (uint32_t)currentMeshParts[part].m_indexStartOffset;
					drawPtr->vertexOffset = meshVertexDataOffset;
					drawPtr->firstInstance = thisInstanceOffset;
					thisInstanceOffset++;
					m_frameStats.m_totalTriangles += currentMeshParts[part].m_indexCount / 3;
				}
				m_frameStats.m_totalModelInstances++;
				m_frameStats.m_totalPartInstances += (uint32_t)currentMeshParts.size();
			}
			return true;
		};
		Entities::Queries::ForEach<StaticMeshComponent, TransformComponent>(activeWorld, forEach);
		return thisInstanceOffset - m_currentInstanceBufferStart;
	}

	bool StaticMeshSimpleRenderer::CollectInstances()
	{
		R3_PROF_EVENT();

		// Reset frame stats
		m_frameStats.m_totalTriangles = m_frameStats.m_totalModelInstances = m_frameStats.m_totalPartInstances = 0;

		// old render writes same num of instances + draws
		uint32_t oldInstancesWritten = WriteInstances();
		m_currentFrameTotalInstances += oldInstancesWritten;
		m_currentFrameTotalDraws += oldInstancesWritten;

		{
			R3_PROF_EVENT("New Stuff");
			CollectModelInstancesFromEntities();
			CollectModelPartInstances();
			PrepareOpaqueDrawData();
		}
		return true;
	}

	void StaticMeshSimpleRenderer::OnMainPassBegin(class RenderPassContext& ctx)
	{
		auto mainColourTarget = ctx.GetResolvedTarget("MainColour");
		auto mainDepthTarget = ctx.GetResolvedTarget("MainDepth");
		MainPassBegin(*ctx.m_device, ctx.m_graphicsCmds, mainColourTarget->m_info.m_format, mainDepthTarget->m_info.m_format);
	}

	void StaticMeshSimpleRenderer::OnMainPassDraw(class RenderPassContext& ctx)
	{
		MainPassDraw(*ctx.m_device, ctx.m_graphicsCmds, ctx.m_renderExtents);
	}

	void StaticMeshSimpleRenderer::MainPassBegin(Device& d, VkCommandBuffer cmds, VkFormat mainColourFormat, VkFormat mainDepthFormat)
	{
		R3_PROF_EVENT();
		if (!InitialiseGpuData(d, mainColourFormat, mainDepthFormat))
		{
			return;
		}
		
		// write + flush the global constants each frame
		auto cameras = GetSystem<CameraSystem>();
		auto staticMeshes = GetSystem<StaticMeshSystem>();
		auto lights = GetSystem<LightsSystem>();
		GlobalConstants globals;
		globals.m_cameraWorldSpacePos = glm::vec4(cameras->GetMainCamera().Position(), 1);
		globals.m_projViewTransform = cameras->GetMainCamera().ProjectionMatrix() * cameras->GetMainCamera().ViewMatrix();
		globals.m_vertexBufferAddress = staticMeshes->GetVertexDataDeviceAddress();
		globals.m_materialBufferAddress = staticMeshes->GetMaterialsDeviceAddress();
		globals.m_pointLightsBufferAddress = lights->GetPointlightsDeviceAddress();
		globals.m_firstPointLightOffset = lights->GetFirstPointlightOffset();
		globals.m_pointlightCount = lights->GetTotalPointlightsThisFrame();
		ProcessEnvironmentSettings(globals);
		m_globalConstantsBuffer.Write(m_currentGlobalConstantsBuffer, 1, &globals);
		m_globalConstantsBuffer.Flush(d, cmds);
		if (++m_currentGlobalConstantsBuffer >= c_maxGlobalConstantBuffers)
		{
			m_currentGlobalConstantsBuffer = 0;
		}
	}

	void StaticMeshSimpleRenderer::MainPassDraw(Device& d, VkCommandBuffer cmds, glm::vec2 extents)
	{
		R3_PROF_EVENT();
		auto entities = Systems::GetSystem<Entities::EntitySystem>();
		auto staticMeshes = GetSystem<StaticMeshSystem>();
		auto textures = GetSystem<TextureSystem>();
		auto time = GetSystem<TimeSystem>();
		if (m_simpleTriPipeline == VK_NULL_HANDLE || entities->GetActiveWorld() == nullptr)
		{
			return;
		}
		m_frameStats.m_writeCmdsStartTime = time->GetElapsedTimeReal();
		VkViewport viewport = { 0 };
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = extents.x;
		viewport.height = extents.y;
		viewport.minDepth = 0.0f;	// normalised! must be between 0 and 1
		viewport.maxDepth = 1.0f;	// ^^
		VkRect2D scissor = { 0 };
		scissor.offset = { 0, 0 };
		scissor.extent = { (uint32_t)viewport.width, (uint32_t)viewport.height };	// draw the full image
		vkCmdBindPipeline(cmds, VK_PIPELINE_BIND_POINT_GRAPHICS, m_simpleTriPipeline);
		vkCmdSetViewport(cmds, 0, 1, &viewport);
		vkCmdSetScissor(cmds, 0, 1, &scissor);

		vkCmdBindIndexBuffer(cmds, staticMeshes->GetIndexBuffer(), 0, VK_INDEX_TYPE_UINT32);
		vkCmdBindDescriptorSets(cmds, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &m_globalDescriptorSet, 0, nullptr);
		VkDescriptorSet allTextures = textures->GetAllTexturesSet();
		if (allTextures != VK_NULL_HANDLE)
		{
			vkCmdBindDescriptorSets(cmds, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 1, 1, &allTextures, 0, nullptr);
		}
		PushConstants pc;
		pc.m_globalConstantIndex = m_currentGlobalConstantsBuffer;
		vkCmdPushConstants(cmds, m_pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pc), &pc);

		// Draw opaques
		size_t drawOffsetBytes = m_opaqueDrawsStart * sizeof(VkDrawIndexedIndirectCommand);
		vkCmdDrawIndexedIndirect(cmds, m_drawIndirectHostVisible.m_buffer, drawOffsetBytes, m_opaqueDrawsCount, sizeof(VkDrawIndexedIndirectCommand));

		// Update instance + draw buffers for next frame
		m_currentInstanceBufferStart += c_maxInstances;
		if (m_currentInstanceBufferStart >= (c_maxInstances * c_maxInstanceBuffers))
		{
			m_currentInstanceBufferStart = 0;
		}
		m_currentFrameTotalInstances = 0;

		m_currentDrawBufferStart += c_maxInstances;
		if (m_currentDrawBufferStart >= (c_maxInstances + c_maxInstanceBuffers))
		{
			m_currentDrawBufferStart = 0;
		}
		m_currentFrameTotalDraws = 0;
		m_frameStats.m_writeCmdsEndTime = time->GetElapsedTimeReal();
	}
}