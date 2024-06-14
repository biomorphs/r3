#include "static_mesh_simple_renderer.h"
#include "static_mesh_system.h"
#include "camera_system.h"
#include "lights_system.h"
#include "texture_system.h"
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
#include "core/profiler.h"
#include "core/log.h"
#include <imgui.h>

constexpr bool c_useHostVisibleDrawBuffers = false;
constexpr bool c_useOwnStaging = false;
constexpr bool c_usePlainOldBuffer = true;

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
		render->m_onMainPassBegin.AddCallback([this](Device& d, VkCommandBuffer cmds) {
			MainPassBegin(d, cmds);
		});
		render->m_onMainPassDraw.AddCallback([this](Device& d, VkCommandBuffer cmds, const VkExtent2D& e) {
			MainPassDraw(d, cmds, e);
		});
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
		m_globalConstantsBuffer.Destroy(d);
		m_globalInstancesBuffer.Destroy(d);
		m_drawIndirectBuffer.Destroy(d);
		m_descriptorAllocator = {};
		vmaUnmapMemory(d.GetVMA(), m_globalInstancesHostVisible.m_allocation);
		vmaDestroyBuffer(d.GetVMA(), m_globalInstancesHostVisible.m_buffer, m_globalInstancesHostVisible.m_allocation);
		vmaUnmapMemory(d.GetVMA(), m_drawIndirectHostVisible.m_allocation);
		vmaDestroyBuffer(d.GetVMA(), m_drawIndirectHostVisible.m_buffer, m_drawIndirectHostVisible.m_allocation);
		vkDestroyDescriptorSetLayout(d.GetVkDevice(), m_globalsDescriptorLayout, nullptr);
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
			ImGui::Begin("Static Mesh Simple Renderer");
			ImGui::End();
		}
		return true;
	}

	bool StaticMeshSimpleRenderer::CreatePipelineData(Device& d)
	{
		R3_PROF_EVENT();

		auto textures = GetSystem<TextureSystem>();

		// Create pipeline layout
		VkPushConstantRange constantRange;
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

		// Load the shaders
		std::string basePath = "shaders_spirv\\common\\";
		auto vertexShader = VulkanHelpers::LoadShaderModule(d.GetVkDevice(), basePath + "static_mesh_simple_main_pass.vert.spv");
		auto fragShader = VulkanHelpers::LoadShaderModule(d.GetVkDevice(), basePath + "static_mesh_simple_main_pass.frag.spv");
		if (vertexShader == VK_NULL_HANDLE || fragShader == VK_NULL_HANDLE)
		{
			LogError("Failed to create shader modules");
			return false;
		}

		// Make the pipeline
		PipelineBuilder pb;
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

		// Enable depth read/write
		VkPipelineDepthStencilStateCreateInfo depthStencilState = { 0 };
		depthStencilState.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		depthStencilState.depthTestEnable = VK_TRUE;
		depthStencilState.depthWriteEnable = VK_TRUE;
		depthStencilState.depthCompareOp = VK_COMPARE_OP_LESS;
		depthStencilState.depthBoundsTestEnable = VK_FALSE;
		depthStencilState.stencilTestEnable = VK_FALSE;
		pb.m_depthStencilState = depthStencilState;

		// No colour attachment blending
		std::vector<VkPipelineColorBlendAttachmentState> allAttachments = {
			VulkanHelpers::CreatePipelineColourBlendAttachment_NoBlending()
		};
		pb.m_colourBlendState = VulkanHelpers::CreatePipelineColourBlendState(allAttachments);	// Pipeline also has some global blending state (constants, logical ops enable)

		auto render = Systems::GetSystem<RenderSystem>();
		auto colourBufferFormat = render->GetMainColourTargetFormat();
		auto depthBufferFormat = render->GetMainDepthStencilFormat();

		// build the pipelines
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
		writer.WriteStorageBuffer(1, c_usePlainOldBuffer ? m_globalInstancesHostVisible.m_buffer : m_globalInstancesBuffer.GetBuffer());
		writer.FlushWrites();

		return true;
	}

	void StaticMeshSimpleRenderer::ProcessEnvironmentSettings(GlobalConstants& g)
	{
		R3_PROF_EVENT();
		auto entities = GetSystem<Entities::EntitySystem>();
		auto w = entities->GetActiveWorld();
		if (w)
		{
			auto getSettings = [&g](const Entities::EntityHandle& e, EnvironmentSettingsComponent& cmp) {
				g.m_sunColourAmbient = { cmp.m_sunColour, cmp.m_sunAmbientFactor };
				g.m_skyColourAmbient = { cmp.m_skyColour, cmp.m_skyAmbientFactor };
				g.m_sunDirectionBrightness = { glm::normalize(cmp.m_sunDirection), cmp.m_sunBrightness };
				return true;
			};
			Entities::Queries::ForEach<EnvironmentSettingsComponent>(w, getSettings);
		}
	}

	bool StaticMeshSimpleRenderer::BuildCommandBuffer()
	{
		R3_PROF_EVENT();
		auto entities = Systems::GetSystem<Entities::EntitySystem>();
		auto activeWorld = entities->GetActiveWorld();
		if (!activeWorld)
		{
			return true;
		}
		auto render = Systems::GetSystem<RenderSystem>();
		auto staticMeshes = GetSystem<StaticMeshSystem>();
		auto cmdBuffer = render->GetCommandBufferAllocator()->CreateCommandBuffer(*render->GetDevice(), false);
		if (!cmdBuffer)
		{
			LogWarn("Failed to get a cmd buffer");
			return false;
		}
		m_thisFrameCmdBuffer = *cmdBuffer;
		VkCommandBufferBeginInfo beginInfo = { 0 };	
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;	// this pass is ran inside another render pass
		VkCommandBufferInheritanceInfo whatIsThisBullshit = { 0 };			// we need to pass the attachments info since we are drawing (annoying)
		whatIsThisBullshit.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
		VkCommandBufferInheritanceRenderingInfoKHR evenMoreBullshit = { 0 };	// dynamic rendering
		evenMoreBullshit.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_RENDERING_INFO_KHR;
		auto colourBufferFormat = render->GetMainColourTargetFormat();
		evenMoreBullshit.colorAttachmentCount = 1;
		evenMoreBullshit.pColorAttachmentFormats = &colourBufferFormat;
		evenMoreBullshit.depthAttachmentFormat = render->GetMainDepthStencilFormat();
		evenMoreBullshit.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
		whatIsThisBullshit.pNext = &evenMoreBullshit;
		beginInfo.pInheritanceInfo = &whatIsThisBullshit;
		if (!VulkanHelpers::CheckResult(vkBeginCommandBuffer(m_thisFrameCmdBuffer.m_cmdBuffer, &beginInfo)))
		{
			LogError("failed to begin recording command buffer!");
			return false;
		}
		VkViewport viewport = { 0 };
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = render->GetWindowExtents().x;
		viewport.height = render->GetWindowExtents().y;
		viewport.minDepth = 0.0f;	// normalised! must be between 0 and 1
		viewport.maxDepth = 1.0f;	// ^^
		VkRect2D scissor = { 0 };
		scissor.offset = { 0, 0 };
		scissor.extent = { (uint32_t)viewport.width, (uint32_t)viewport.height };	// draw the full image
		vkCmdBindPipeline(m_thisFrameCmdBuffer.m_cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_simpleTriPipeline);
		vkCmdSetViewport(m_thisFrameCmdBuffer.m_cmdBuffer, 0, 1, &viewport);
		vkCmdSetScissor(m_thisFrameCmdBuffer.m_cmdBuffer, 0, 1, &scissor);
		vkCmdBindIndexBuffer(m_thisFrameCmdBuffer.m_cmdBuffer, staticMeshes->GetIndexBuffer(), 0, VK_INDEX_TYPE_UINT32);
		vkCmdBindDescriptorSets(m_thisFrameCmdBuffer.m_cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &m_globalDescriptorSet, 0, nullptr);
		auto textures = GetSystem<TextureSystem>();
		VkDescriptorSet allTextures = textures->GetAllTexturesSet();
		if (allTextures != VK_NULL_HANDLE)
		{
			vkCmdBindDescriptorSets(m_thisFrameCmdBuffer.m_cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 1, 1, &allTextures, 0, nullptr);
		}
		PushConstants pc;
		pc.m_globalConstantIndex = m_currentGlobalConstantsBuffer++;
		if (m_currentGlobalConstantsBuffer >= c_maxGlobalConstantBuffers)
		{
			m_currentGlobalConstantsBuffer = 0;
		}
		vkCmdPushConstants(m_thisFrameCmdBuffer.m_cmdBuffer, m_pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pc), &pc);

		static std::vector<StaticMeshInstanceGpu> gpuInstanceData;	// so we can do 1 big write to gpu memory
		gpuInstanceData.clear();
		static std::vector<VkDrawIndexedIndirectCommand> drawCalls;	// ^^
		drawCalls.clear();

		// Try to cache what we can while iterating, yes this is messy, but its fast!
		StaticMeshGpuData currentMeshData;
		ModelDataHandle currentMeshDataHandle;
		std::vector<StaticMeshPart> currentMeshParts;
		StaticMeshMaterialsComponent* lastMatCmp = nullptr;
		uint32_t lastMatCmpGpuIndex = -1;
		uint32_t lastMatCmpOverrideCount = -1;
		Entities::EntityHandle lastMatEntity;
		uint32_t firstInstanceOffset = m_currentInstanceBufferStart;
		m_currentInstanceBufferStart += c_maxInstances;
		if (m_currentInstanceBufferStart >= (c_maxInstances * c_maxInstanceBuffers))
		{
			m_currentInstanceBufferStart = 0;
		}
		uint32_t thisInstanceOffset = firstInstanceOffset;
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
					lastMatCmp = activeWorld->GetComponent<StaticMeshMaterialsComponent>(s.m_materialOverride);
					lastMatCmpGpuIndex = lastMatCmp ? (uint32_t)lastMatCmp->m_gpuDataIndex : -1;
					lastMatCmpOverrideCount = lastMatCmp ? (uint32_t)lastMatCmp->m_materials.size() : 0;
					lastMatEntity = s.m_materialOverride;
				}
				StaticMeshInstanceGpu gpuData;
				VkDrawIndexedIndirectCommand drawData;
				const uint32_t meshMaterialIndex = currentMeshData.m_materialGpuIndex;
				const uint32_t meshVertexDataOffset = (uint32_t)currentMeshData.m_vertexDataOffset;
				const glm::mat4 compTransform = t.GetWorldspaceInterpolated();
				const bool useOverrides = lastMatCmpGpuIndex != -1 && lastMatCmpOverrideCount >= currentMeshData.m_materialCount;
				for (uint32_t part = 0; part < currentMeshParts.size(); ++part)
				{
					auto relativePartMatIndex = currentMeshParts[part].m_materialIndex - meshMaterialIndex;
					gpuData.m_materialIndex = useOverrides ? (lastMatCmpGpuIndex + relativePartMatIndex) : currentMeshParts[part].m_materialIndex;
					gpuData.m_transform = compTransform * currentMeshParts[part].m_transform;
					drawData.indexCount = currentMeshParts[part].m_indexCount;
					drawData.instanceCount = 1;
					drawData.firstIndex = (uint32_t)currentMeshParts[part].m_indexStartOffset;
					drawData.vertexOffset = meshVertexDataOffset;
					drawData.firstInstance = thisInstanceOffset;
					if constexpr (c_usePlainOldBuffer)
					{
						StaticMeshInstanceGpu* instancePtr = static_cast<StaticMeshInstanceGpu*>(m_globalInstancesMappedPtr) + thisInstanceOffset;
						VkDrawIndexedIndirectCommand* drawPtr = static_cast<VkDrawIndexedIndirectCommand*>(m_drawIndirectMappedPtr) + thisInstanceOffset;
						*instancePtr = gpuData;
						*drawPtr = drawData;
					}
					else if constexpr (c_useOwnStaging)
					{
						gpuInstanceData.push_back(gpuData);
						drawCalls.push_back(drawData);
					}
					else
					{
						m_globalInstancesBuffer.Write(thisInstanceOffset, 1, &gpuData);
						m_drawIndirectBuffer.Write(thisInstanceOffset, 1, &drawData);
					}
					thisInstanceOffset++;
				}
			}
			return true;
		};
		Entities::Queries::ForEach<StaticMeshComponent, TransformComponent>(activeWorld, forEach);

		if constexpr (c_useOwnStaging)
		{
			m_globalInstancesBuffer.Write(firstInstanceOffset, (uint32_t)gpuInstanceData.size(), gpuInstanceData.data());
			m_drawIndirectBuffer.Write(firstInstanceOffset, (uint32_t)drawCalls.size(), drawCalls.data());
		}

		size_t drawOffsetBytes = firstInstanceOffset * sizeof(VkDrawIndexedIndirectCommand);
		vkCmdDrawIndexedIndirect(m_thisFrameCmdBuffer.m_cmdBuffer, c_usePlainOldBuffer ? m_drawIndirectHostVisible.m_buffer : m_drawIndirectBuffer.GetBuffer(), drawOffsetBytes, thisInstanceOffset - firstInstanceOffset, sizeof(VkDrawIndexedIndirectCommand));

		if (!VulkanHelpers::CheckResult(vkEndCommandBuffer(m_thisFrameCmdBuffer.m_cmdBuffer)))
		{
			LogError("failed to end recording command buffer!");
			return false;
		}
		
		return true;
	}

	void StaticMeshSimpleRenderer::MainPassBegin(Device& d, VkCommandBuffer cmds)
	{
		R3_PROF_EVENT();
		if (!m_globalConstantsBuffer.IsCreated())
		{
			if (!m_globalConstantsBuffer.Create(d, c_maxGlobalConstantBuffers, c_maxGlobalConstantBuffers, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT))
			{
				LogError("Failed to create constant buffer");
			}
			else
			{
				m_globalConstantsBuffer.Allocate(c_maxGlobalConstantBuffers);	// reserve the memory now, allows writes to any entry
			}
		}
		if (!m_drawIndirectBuffer.IsCreated())
		{
			if (!m_drawIndirectBuffer.Create(d, c_maxInstances * c_maxInstanceBuffers, c_maxInstances, VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, c_useHostVisibleDrawBuffers))
			{
				LogError("Failed to create draw indirect buffer");
			}
			else
			{
				m_drawIndirectBuffer.Allocate(c_maxInstances * c_maxInstanceBuffers);	// reserve the memory now, allows writes to any entry
			}
		}
		if (!m_globalInstancesBuffer.IsCreated())
		{
			if (!m_globalInstancesBuffer.Create(d, c_maxInstances * c_maxInstanceBuffers, c_maxInstances, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, c_useHostVisibleDrawBuffers))
			{
				LogError("Failed to create constant buffer");
			}
			else
			{
				m_globalInstancesBuffer.Allocate(c_maxInstances * c_maxInstanceBuffers);	// reserve the memory now, allows writes to any entry
			}
		}
		if constexpr (c_usePlainOldBuffer)
		{
			if (m_globalInstancesMappedPtr == nullptr)
			{
				m_globalInstancesHostVisible = VulkanHelpers::CreateBuffer(d.GetVMA(),
					c_maxInstances * c_maxInstanceBuffers * sizeof(StaticMeshInstanceGpu),
					VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_AUTO, VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);
				vmaMapMemory(d.GetVMA(), m_globalInstancesHostVisible.m_allocation, &m_globalInstancesMappedPtr);
			}
			if (m_drawIndirectMappedPtr == nullptr)
			{
				m_drawIndirectHostVisible = VulkanHelpers::CreateBuffer(d.GetVMA(),
					c_maxInstances * c_maxInstanceBuffers * sizeof(VkDrawIndexedIndirectCommand),
					VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT, VMA_MEMORY_USAGE_AUTO, VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);
				vmaMapMemory(d.GetVMA(), m_drawIndirectHostVisible.m_allocation, &m_drawIndirectMappedPtr);
			}
		}
		if (m_globalDescriptorSet == VK_NULL_HANDLE)
		{
			if (!CreateGlobalDescriptorSet())
			{
				LogError("Failed to create global descriptor set");
			}
		}
		if (m_pipelineLayout == VK_NULL_HANDLE || m_simpleTriPipeline == VK_NULL_HANDLE)
		{
			CreatePipelineData(d);
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

		// flush the instance data before drawing 
		m_globalInstancesBuffer.Flush(d, cmds);
		m_drawIndirectBuffer.Flush(d, cmds, VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT);
	}

	void StaticMeshSimpleRenderer::MainPassDraw(Device& d, VkCommandBuffer cmds, const VkExtent2D& e)
	{
		R3_PROF_EVENT();
		if (m_simpleTriPipeline == VK_NULL_HANDLE)
		{
			return;
		}

		// submit secondary cmd buffer
		if (m_thisFrameCmdBuffer.m_cmdBuffer)
		{
			vkCmdExecuteCommands(cmds, 1, &m_thisFrameCmdBuffer.m_cmdBuffer);
		}

		auto render = Systems::GetSystem<RenderSystem>();
		render->GetCommandBufferAllocator()->Release(m_thisFrameCmdBuffer);
		m_thisFrameCmdBuffer = {};
	}
}