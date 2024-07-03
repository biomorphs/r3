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
#include "render/render_helpers.h"
#include "render/render_pass_context.h"
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
		RegisterTick("StaticMeshSimpleRenderer::BuildCommandBuffer", [this]() {
			return BuildCommandBuffer();
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

	bool StaticMeshSimpleRenderer::CreatePipelineData(Device& d)
	{
		R3_PROF_EVENT();
		auto textures = GetSystem<TextureSystem>();
		auto render = Systems::GetSystem<RenderSystem>();
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
		auto colourBufferFormat = render->GetMainColourTargetFormat();
		auto depthBufferFormat = render->GetMainDepthStencilFormat();
		m_simpleTriPipeline = pb.Build(d.GetVkDevice(), m_pipelineLayout, 1, &colourBufferFormat, depthBufferFormat);
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

	bool StaticMeshSimpleRenderer::InitialiseGpuData(Device& d)
	{
		if (!m_globalConstantsBuffer.IsCreated())
		{
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
			void* mapped = nullptr;
			vmaMapMemory(d.GetVMA(), m_globalInstancesHostVisible.m_allocation, &mapped);
			m_globalInstancesMappedPtr = static_cast<StaticMeshInstanceGpu*>(mapped);
		}
		if (m_drawIndirectMappedPtr == nullptr)
		{
			m_drawIndirectHostVisible = VulkanHelpers::CreateBuffer(d.GetVMA(),
				c_maxInstances * c_maxInstanceBuffers * sizeof(VkDrawIndexedIndirectCommand),
				VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT, VMA_MEMORY_USAGE_AUTO, VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);
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
			CreatePipelineData(d);
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

	uint32_t StaticMeshSimpleRenderer::WriteInstances(VkCommandBuffer_T* buffer)
	{
		R3_PROF_EVENT();
		auto entities = Systems::GetSystem<Entities::EntitySystem>();
		auto render = Systems::GetSystem<RenderSystem>();
		auto staticMeshes = GetSystem<StaticMeshSystem>();
		auto textures = GetSystem<TextureSystem>();
		auto activeWorld = entities->GetActiveWorld();
		StaticMeshGpuData currentMeshData;				// Try to cache what we can while iterating, yes this is messy, but its fast!
		ModelDataHandle currentMeshDataHandle;
		std::vector<StaticMeshPart> currentMeshParts;	// cache the parts to avoid touching the meshes constantly
		uint32_t lastMatOverrideGpuIndex = -1;			// index into gpu materials from the last material component
		Entities::EntityHandle lastMatEntity;
		uint32_t thisInstanceOffset = m_currentInstanceBufferStart;
		auto forEach = [&](const Entities::EntityHandle& e, StaticMeshComponent& s, TransformComponent& t)
		{
			if (s.m_modelHandle.m_index == -1)
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
				const glm::mat4 compTransform = t.GetWorldspaceInterpolated();
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

	bool StaticMeshSimpleRenderer::BuildCommandBuffer()
	{
		R3_PROF_EVENT();
		m_frameStats = {};
		auto entities = Systems::GetSystem<Entities::EntitySystem>();
		auto render = Systems::GetSystem<RenderSystem>();
		auto staticMeshes = GetSystem<StaticMeshSystem>();
		auto textures = GetSystem<TextureSystem>();
		auto time = GetSystem<TimeSystem>();
		if (!entities->GetActiveWorld())
		{
			return true;
		}
		m_frameStats.m_writeCmdsStartTime = time->GetElapsedTimeReal();
		auto cmdBuffer = render->GetCommandBufferAllocator()->CreateCommandBuffer(*render->GetDevice(), false);
		if (!cmdBuffer)
		{
			LogWarn("Failed to get a cmd buffer");
			return false;
		}
		m_thisFrameCmdBuffer = *cmdBuffer;
		if (!RenderHelpers::BeginSecondaryCommandBuffer(m_thisFrameCmdBuffer.m_cmdBuffer))
		{
			LogError("Failed to begin writing cmds");
			return false;
		}
		RenderHelpers::BindPipeline(m_thisFrameCmdBuffer.m_cmdBuffer, m_simpleTriPipeline);
		vkCmdBindIndexBuffer(m_thisFrameCmdBuffer.m_cmdBuffer, staticMeshes->GetIndexBuffer(), 0, VK_INDEX_TYPE_UINT32);
		vkCmdBindDescriptorSets(m_thisFrameCmdBuffer.m_cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &m_globalDescriptorSet, 0, nullptr);
		VkDescriptorSet allTextures = textures->GetAllTexturesSet();
		if (allTextures != VK_NULL_HANDLE)
		{
			vkCmdBindDescriptorSets(m_thisFrameCmdBuffer.m_cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 1, 1, &allTextures, 0, nullptr);
		}
		PushConstants pc;
		pc.m_globalConstantIndex = m_currentGlobalConstantsBuffer;
		vkCmdPushConstants(m_thisFrameCmdBuffer.m_cmdBuffer, m_pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pc), &pc);
		uint32_t drawCount = WriteInstances(m_thisFrameCmdBuffer.m_cmdBuffer);
		size_t drawOffsetBytes = m_currentInstanceBufferStart * sizeof(VkDrawIndexedIndirectCommand);
		vkCmdDrawIndexedIndirect(m_thisFrameCmdBuffer.m_cmdBuffer, m_drawIndirectHostVisible.m_buffer, drawOffsetBytes, drawCount, sizeof(VkDrawIndexedIndirectCommand));
		m_currentInstanceBufferStart += c_maxInstances;
		if (m_currentInstanceBufferStart >= (c_maxInstances * c_maxInstanceBuffers))
		{
			m_currentInstanceBufferStart = 0;
		}
		if (!VulkanHelpers::CheckResult(vkEndCommandBuffer(m_thisFrameCmdBuffer.m_cmdBuffer)))
		{
			LogError("failed to end recording command buffer!");
			return false;
		}
		m_frameStats.m_writeCmdsEndTime = time->GetElapsedTimeReal();
		return true;
	}

	void StaticMeshSimpleRenderer::OnMainPassBegin(class RenderPassContext& ctx)
	{
		MainPassBegin(*ctx.m_device, ctx.m_graphicsCmds);
	}

	void StaticMeshSimpleRenderer::OnMainPassDraw(class RenderPassContext& ctx)
	{
		MainPassDraw(*ctx.m_device, ctx.m_graphicsCmds);
	}

	void StaticMeshSimpleRenderer::MainPassBegin(Device& d, VkCommandBuffer cmds)
	{
		R3_PROF_EVENT();
		if (!InitialiseGpuData(d))
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

	void StaticMeshSimpleRenderer::MainPassDraw(Device& d, VkCommandBuffer cmds)
	{
		R3_PROF_EVENT();
		if (m_simpleTriPipeline == VK_NULL_HANDLE)
		{
			return;
		}

		// submit secondary cmd buffer
		if (m_thisFrameCmdBuffer.m_cmdBuffer)
		{
			R3_PROF_GPU_EVENT("StaticMeshSimpleRenderer::MainPassDraw");
			vkCmdExecuteCommands(cmds, 1, &m_thisFrameCmdBuffer.m_cmdBuffer);
		}

		auto render = Systems::GetSystem<RenderSystem>();
		render->GetCommandBufferAllocator()->Release(m_thisFrameCmdBuffer);
		m_thisFrameCmdBuffer = {};
	}
}