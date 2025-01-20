#include "static_mesh_renderer.h"
#include "static_mesh_system.h"
#include "camera_system.h"
#include "lights_system.h"
#include "texture_system.h"
#include "time_system.h"
#include "engine/utils/frustum.h"
#include "engine/ui/imgui_menubar_helper.h"
#include "engine/graphics/static_mesh_instance_culling_compute.h"
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
#include "render/render_pass_context.h"
#include "render/render_target_cache.h"
#include "core/profiler.h"
#include "core/log.h"
#include <imgui.h>

#define ENABLE_CPU_INSTANCE_CULLING
#define ENABLE_ASYNC_CPU_INSTANCE_CULLING

// CPU Culling notes
// Cost of populating 20k cube instances
// with ENABLE_CPU_INSTANCE_CULLING - 0.78ms to ~1.89ms
// no ENABLE_CPU_INSTANCE_CULLING - 0.68ms to ~1.68ms
// 0.1 to 0.2ms to populate instance data

// Culling time for 20k instances
//	ENABLE_ASYNC_CPU_INSTANCE_CULLING - ~0.5ms

namespace R3
{
	// stored in a buffer
	struct StaticMeshRenderer::GlobalConstants
	{
		glm::mat4 m_projViewTransform;
		glm::vec4 m_cameraWorldSpacePos;
		VkDeviceAddress m_vertexBufferAddress;
		VkDeviceAddress m_materialBufferAddress;
		VkDeviceAddress m_lightDataBufferAddress;
		VkDeviceAddress m_instanceDataBufferAddress;
	};

	// pushed once per pipeline, provides global constants buffer address for this frame
	struct PushConstants 
	{
		VkDeviceAddress m_globalsBufferAddress;
	};

	StaticMeshRenderer::StaticMeshRenderer()
	{
#ifdef ENABLE_CPU_INSTANCE_CULLING
		m_globalInstancesCPU.resize(c_maxInstances);
#endif
		m_computeCulling = std::make_unique<StaticMeshInstanceCullingCompute>();
	}

	StaticMeshRenderer::~StaticMeshRenderer()
	{
	}

	void StaticMeshRenderer::RegisterTickFns()
	{
		R3_PROF_EVENT();
		RegisterTick("StaticMeshRenderer::ShowGui", [this]() {
			return ShowGui();
		});
		RegisterTick("StaticMeshRenderer::CollectInstances", [this]() {
			return CollectInstances();
		});
	}

	bool StaticMeshRenderer::Init()
	{
		R3_PROF_EVENT();
		auto render = Systems::GetSystem<RenderSystem>();
		render->m_onShutdownCbs.AddCallback([this](Device& d) {
			Cleanup(d);
		});
		return true;
	}

	void StaticMeshRenderer::Cleanup(Device& d)
	{
		R3_PROF_EVENT();
		m_computeCulling->Cleanup(d);
		vkDestroyPipeline(d.GetVkDevice(), m_forwardPipeline, nullptr);
		vkDestroyPipeline(d.GetVkDevice(), m_gBufferPipeline, nullptr);
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
		m_globalConstantsBuffer.Destroy(d);
	}

	bool StaticMeshRenderer::ShowGui()
	{
		R3_PROF_EVENT();
		auto& debugMenu = MenuBar::MainMenu().GetSubmenu("Debug");
		debugMenu.AddItem("Static Mesh Renderer", [this]() {
			m_showGui = !m_showGui;
		});
		if (m_showGui)
		{
			ImGui::Begin("Static Mesh Renderer");
			ImGui::Checkbox("Forward render everything", &m_forwardRenderEverything);
			ImGui::Checkbox("Enable Compute Culling", &m_enableComputeCulling);
			ImGui::Checkbox("Enable CPU Culling", &m_enableCpuCulling);
			std::string txt = std::format("{} Model Instances Submitted", m_frameStats.m_totalModelInstances);
			ImGui::Text(txt.c_str());
			txt = std::format("    {} Total Part Instances", m_frameStats.m_totalPartInstances);
			ImGui::Text(txt.c_str());
			txt = std::format("    {} Opaque Part Instances", m_frameStats.m_totalOpaqueInstances);
			ImGui::Text(txt.c_str());
			txt = std::format("    {} Transparent Part Instances", m_frameStats.m_totalTransparentInstances);
			ImGui::Text(txt.c_str());
			txt = std::format("    {:L} Triangles", m_frameStats.m_totalTriangles);
			ImGui::Text(txt.c_str());
			txt = std::format("Part instances took {:.3f}ms to collect", 1000.0 * (m_frameStats.m_collectInstancesEndTime - m_frameStats.m_collectInstancesStartTime));
			ImGui::Text(txt.c_str());
			txt = std::format("Draw buckets took {:.3f}ms to prepare", 1000.0 * (m_frameStats.m_prepareBucketsEndTime - m_frameStats.m_prepareBucketsStartTime));
			ImGui::Text(txt.c_str());
			txt = std::format("Command buffer took {:.3f}ms to write", 1000.0 * (m_frameStats.m_writeCmdsEndTime - m_frameStats.m_writeCmdsStartTime));
			ImGui::Text(txt.c_str());
			ImGui::End();
		}
		return true;
	}

	bool StaticMeshRenderer::CreatePipelineLayout(Device& d)
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
		VkDescriptorSetLayout setLayouts[] = { textures->GetDescriptorsLayout() };
		pipelineLayoutInfo.pSetLayouts = setLayouts;
		pipelineLayoutInfo.setLayoutCount = (uint32_t)std::size(setLayouts);
		if (!VulkanHelpers::CheckResult(vkCreatePipelineLayout(d.GetVkDevice(), &pipelineLayoutInfo, nullptr, &m_pipelineLayout)))
		{
			LogError("Failed to create pipeline layout");
			return false;
		}
		return true;
	}

	bool StaticMeshRenderer::CreateGBufferPipelineData(Device& d, VkFormat positionMetalFormat, VkFormat normalRoughnessFormat, VkFormat albedoAOFormat, VkFormat mainDepthFormat)
	{
		R3_PROF_EVENT();
		std::string basePath = "shaders_spirv\\common\\";	// Load the shaders
		auto vertexShader = VulkanHelpers::LoadShaderModule(d.GetVkDevice(), basePath + "static_mesh.vert.spv");
		auto fragShader = VulkanHelpers::LoadShaderModule(d.GetVkDevice(), basePath + "static_mesh_gbuffer.frag.spv");
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

		// set up gbuffer attachments
		VkFormat colourAttachFormats[] =
		{
			positionMetalFormat,
			normalRoughnessFormat,
			albedoAOFormat
		};
		std::vector<VkPipelineColorBlendAttachmentState> allAttachments = {	// No colour attachment blending
			VulkanHelpers::CreatePipelineColourBlendAttachment_NoBlending(),
			VulkanHelpers::CreatePipelineColourBlendAttachment_NoBlending(),
			VulkanHelpers::CreatePipelineColourBlendAttachment_NoBlending()
		};
		pb.m_colourBlendState = VulkanHelpers::CreatePipelineColourBlendState(allAttachments);
		m_gBufferPipeline = pb.Build(d.GetVkDevice(), m_pipelineLayout, (int)std::size(colourAttachFormats), colourAttachFormats, mainDepthFormat);
		if (m_gBufferPipeline == VK_NULL_HANDLE)
		{
			LogError("Failed to create pipeline!");
			return false;
		}
		vkDestroyShaderModule(d.GetVkDevice(), vertexShader, nullptr);
		vkDestroyShaderModule(d.GetVkDevice(), fragShader, nullptr);
		return true;
	}

	bool StaticMeshRenderer::CreateForwardPipelineData(Device& d, VkFormat mainColourFormat, VkFormat mainDepthFormat)
	{
		R3_PROF_EVENT();
		std::string basePath = "shaders_spirv\\common\\";	// Load the shaders
		auto vertexShader = VulkanHelpers::LoadShaderModule(d.GetVkDevice(), basePath + "static_mesh.vert.spv");
		auto fragShader = VulkanHelpers::LoadShaderModule(d.GetVkDevice(), basePath + "static_mesh_forward.frag.spv");
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
		depthStencilState.depthWriteEnable = VK_FALSE;						// don't write to depth in forward pass
		depthStencilState.depthCompareOp = VK_COMPARE_OP_LESS;
		depthStencilState.depthBoundsTestEnable = VK_FALSE;
		depthStencilState.stencilTestEnable = VK_FALSE;
		pb.m_depthStencilState = depthStencilState;
		std::vector<VkPipelineColorBlendAttachmentState> allAttachments = {	
			VulkanHelpers::CreatePipelineColourBlendAttachment_AlphaBlending()	//alpha blending
		};
		pb.m_colourBlendState = VulkanHelpers::CreatePipelineColourBlendState(allAttachments);
		m_forwardPipeline = pb.Build(d.GetVkDevice(), m_pipelineLayout, 1, &mainColourFormat, mainDepthFormat);
		if (m_forwardPipeline == VK_NULL_HANDLE)
		{
			LogError("Failed to create pipeline!");
			return false;
		}
		vkDestroyShaderModule(d.GetVkDevice(), vertexShader, nullptr);
		vkDestroyShaderModule(d.GetVkDevice(), fragShader, nullptr);
		return true;
	}

	void StaticMeshRenderer::CullInstancesOnGpu(RenderPassContext& ctx)
	{
		R3_PROF_EVENT();
		// Run compute instance prep+culling here for now (may want a separate frame graph pass instead?)
		if (m_enableComputeCulling)
		{
			m_frameStats.m_prepareBucketsStartTime = GetSystem<TimeSystem>()->GetElapsedTimeReal();
			PrepareAndCullDrawBucketCompute(*ctx.m_device, ctx.m_graphicsCmds, m_allOpaques);
			PrepareAndCullDrawBucketCompute(*ctx.m_device, ctx.m_graphicsCmds, m_allTransparents);
			m_frameStats.m_prepareBucketsEndTime = GetSystem<TimeSystem>()->GetElapsedTimeReal();
		}
	}

	void StaticMeshRenderer::PrepareForRendering(class RenderPassContext& ctx)
	{
		R3_PROF_EVENT();
		if (!m_globalConstantsBuffer.IsCreated())
		{
			m_globalConstantsBuffer.SetDebugName("Static mesh global constants");
			if (!m_globalConstantsBuffer.Create(*ctx.m_device, c_maxGlobalConstantBuffers, 1, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT))
			{
				LogError("Failed to create constant buffer");
				return;
			}
			m_globalConstantsBuffer.Allocate(c_maxGlobalConstantBuffers);
		}
		if (m_globalInstancesMappedPtr == nullptr)
		{
			m_globalInstancesHostVisible = VulkanHelpers::CreateBuffer(ctx.m_device->GetVMA(),
				c_maxInstances * c_maxInstanceBuffers * sizeof(StaticMeshInstanceGpu),
				VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VMA_MEMORY_USAGE_AUTO, VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);
			VulkanHelpers::SetBufferName(ctx.m_device->GetVkDevice(), m_globalInstancesHostVisible, "Static mesh global instances");
			void* mapped = nullptr;
			vmaMapMemory(ctx.m_device->GetVMA(), m_globalInstancesHostVisible.m_allocation, &mapped);
			m_globalInstancesMappedPtr = static_cast<StaticMeshInstanceGpu*>(mapped);
			m_globalInstancesDeviceAddress = VulkanHelpers::GetBufferDeviceAddress(ctx.m_device->GetVkDevice(), m_globalInstancesHostVisible);
		}
		if (m_drawIndirectMappedPtr == nullptr)
		{
			m_drawIndirectHostVisible = VulkanHelpers::CreateBuffer(ctx.m_device->GetVMA(),
				c_maxInstances * c_maxInstanceBuffers * sizeof(VkDrawIndexedIndirectCommand),
				VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VMA_MEMORY_USAGE_AUTO, VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);
			VulkanHelpers::SetBufferName(ctx.m_device->GetVkDevice(), m_drawIndirectHostVisible, "Static mesh draw indirect");
			void* mapped = nullptr;
			vmaMapMemory(ctx.m_device->GetVMA(), m_drawIndirectHostVisible.m_allocation, &mapped);
			m_drawIndirectBufferAddress = VulkanHelpers::GetBufferDeviceAddress(ctx.m_device->GetVkDevice(), m_drawIndirectHostVisible);
			m_drawIndirectMappedPtr = static_cast<StaticMeshInstanceGpu*>(mapped);
		}
		if (m_pipelineLayout == VK_NULL_HANDLE)
		{
			if (!CreatePipelineLayout(*ctx.m_device))
			{
				LogError("Failed to create static mesh pipeline layout");
				return;
			}
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
		globals.m_lightDataBufferAddress = lights->GetAllLightsDeviceAddress();
		globals.m_instanceDataBufferAddress = m_globalInstancesDeviceAddress;
		m_globalConstantsBuffer.Write(m_currentGlobalConstantsBuffer, 1, &globals);
		m_globalConstantsBuffer.Flush(*ctx.m_device, ctx.m_graphicsCmds, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
	}

	void StaticMeshRenderer::OnGBufferPassDraw(class RenderPassContext& ctx)
	{
		R3_PROF_EVENT();

		if (m_gBufferPipeline == VK_NULL_HANDLE)
		{
			auto positionMetalTarget = ctx.GetResolvedTarget("GBuffer_PositionMetallic");
			auto normalRoughnessTarget = ctx.GetResolvedTarget("GBuffer_NormalsRoughness");
			auto albedoAOTarget = ctx.GetResolvedTarget("GBuffer_AlbedoAO");
			auto mainDepthTarget = ctx.GetResolvedTarget("MainDepth");
			if (!CreateGBufferPipelineData(*ctx.m_device, positionMetalTarget->m_info.m_format, normalRoughnessTarget->m_info.m_format, albedoAOTarget->m_info.m_format, mainDepthTarget->m_info.m_format))
			{
				LogError("Failed to create pipeline data for gbuffer");
			}
		}

		auto textures = GetSystem<TextureSystem>();
		auto time = GetSystem<TimeSystem>();
		if (m_allOpaques.m_drawCount == 0)
		{
			return;
		}

		m_frameStats.m_writeCmdsStartTime = time->GetElapsedTimeReal();

		VkViewport viewport = { 0 };
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = ctx.m_renderExtents.x;
		viewport.height = ctx.m_renderExtents.y;
		viewport.minDepth = 0.0f;	// normalised! must be between 0 and 1
		viewport.maxDepth = 1.0f;	// ^^
		VkRect2D scissor = { 0 };
		scissor.offset = { 0, 0 };
		scissor.extent = { (uint32_t)viewport.width, (uint32_t)viewport.height };	// draw the full image
		vkCmdBindPipeline(ctx.m_graphicsCmds, VK_PIPELINE_BIND_POINT_GRAPHICS, m_gBufferPipeline);
		vkCmdSetViewport(ctx.m_graphicsCmds, 0, 1, &viewport);
		vkCmdSetScissor(ctx.m_graphicsCmds, 0, 1, &scissor);
		vkCmdBindIndexBuffer(ctx.m_graphicsCmds, GetSystem<StaticMeshSystem>()->GetIndexBuffer(), 0, VK_INDEX_TYPE_UINT32);
		VkDescriptorSet allTextures = textures->GetAllTexturesSet();
		vkCmdBindDescriptorSets(ctx.m_graphicsCmds, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &allTextures, 0, nullptr);
		PushConstants pc;
		pc.m_globalsBufferAddress = m_globalConstantsBuffer.GetDataDeviceAddress() + (m_currentGlobalConstantsBuffer * sizeof(GlobalConstants));
		vkCmdPushConstants(ctx.m_graphicsCmds, m_pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pc), &pc);

		// Draw opaques
		vkCmdDrawIndexedIndirect(ctx.m_graphicsCmds, m_drawIndirectHostVisible.m_buffer, m_allOpaques.m_firstDrawOffset * sizeof(VkDrawIndexedIndirectCommand), m_allOpaques.m_drawCount, sizeof(VkDrawIndexedIndirectCommand));

		m_frameStats.m_writeCmdsEndTime = time->GetElapsedTimeReal();
	}

	void StaticMeshRenderer::OnForwardPassDraw(class RenderPassContext& ctx)
	{
		R3_PROF_EVENT();

		if (m_forwardPipeline == VK_NULL_HANDLE)
		{
			auto mainColourTarget = ctx.GetResolvedTarget("MainColour");
			auto mainDepthTarget = ctx.GetResolvedTarget("MainDepth");
			if (!CreateForwardPipelineData(*ctx.m_device, mainColourTarget->m_info.m_format, mainDepthTarget->m_info.m_format))
			{
				LogError("Failed to create pipeline data for forward pass");
			}
		}

		auto textures = GetSystem<TextureSystem>();
		auto time = GetSystem<TimeSystem>();
		if (m_allTransparents.m_drawCount == 0)
		{
			return;
		}

		m_frameStats.m_writeCmdsStartTime = time->GetElapsedTimeReal();

		VkViewport viewport = { 0 };
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = ctx.m_renderExtents.x;
		viewport.height = ctx.m_renderExtents.y;
		viewport.minDepth = 0.0f;	// normalised! must be between 0 and 1
		viewport.maxDepth = 1.0f;	// ^^
		VkRect2D scissor = { 0 };
		scissor.offset = { 0, 0 };
		scissor.extent = { (uint32_t)viewport.width, (uint32_t)viewport.height };	// draw the full image
		vkCmdBindPipeline(ctx.m_graphicsCmds, VK_PIPELINE_BIND_POINT_GRAPHICS, m_forwardPipeline);
		vkCmdSetViewport(ctx.m_graphicsCmds, 0, 1, &viewport);
		vkCmdSetScissor(ctx.m_graphicsCmds, 0, 1, &scissor);
		vkCmdBindIndexBuffer(ctx.m_graphicsCmds, GetSystem<StaticMeshSystem>()->GetIndexBuffer(), 0, VK_INDEX_TYPE_UINT32);
		VkDescriptorSet allTextures = textures->GetAllTexturesSet();
		vkCmdBindDescriptorSets(ctx.m_graphicsCmds, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &allTextures, 0, nullptr);
		PushConstants pc;
		pc.m_globalsBufferAddress = m_globalConstantsBuffer.GetDataDeviceAddress() + (m_currentGlobalConstantsBuffer * sizeof(GlobalConstants));
		vkCmdPushConstants(ctx.m_graphicsCmds, m_pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pc), &pc);

		// Draw opaques
		vkCmdDrawIndexedIndirect(ctx.m_graphicsCmds, m_drawIndirectHostVisible.m_buffer, m_allTransparents.m_firstDrawOffset * sizeof(VkDrawIndexedIndirectCommand), m_allTransparents.m_drawCount, sizeof(VkDrawIndexedIndirectCommand));

		m_frameStats.m_writeCmdsEndTime = time->GetElapsedTimeReal();
	}

	void StaticMeshRenderer::OnDrawEnd(class RenderPassContext& ctx)
	{
		R3_PROF_EVENT();

		// Update draw buffers for next frame
		m_currentInstanceBufferStart += c_maxInstances;
		if (m_currentInstanceBufferStart >= (c_maxInstances * c_maxInstanceBuffers))
		{
			m_currentInstanceBufferStart = 0;
		}
		m_currentInstanceBufferOffset = 0;

		m_currentDrawBufferStart += c_maxInstances;
		if (m_currentDrawBufferStart >= (c_maxInstances * c_maxInstanceBuffers))
		{
			m_currentDrawBufferStart = 0;
		}
		m_currentDrawBufferOffset = 0;

		m_currentGlobalConstantsBuffer++;
		if (m_currentGlobalConstantsBuffer >= c_maxGlobalConstantBuffers)
		{
			m_currentGlobalConstantsBuffer = 0;
		}

		m_computeCulling->Reset();
	}

	VkDeviceAddress StaticMeshRenderer::GetDrawIndirectBufferAddress()
	{
		return m_drawIndirectBufferAddress;
	}

	VkDeviceAddress StaticMeshRenderer::GetPerDrawInstanceBufferAddress()
	{
		return m_globalInstancesDeviceAddress;
	}

	void StaticMeshRenderer::CollectAllPartInstances()
	{
		R3_PROF_EVENT();
		auto staticMeshes = GetSystem<StaticMeshSystem>();
		auto activeWorld = Systems::GetSystem<Entities::EntitySystem>()->GetActiveWorld();
		m_allOpaques.m_partInstances.clear();
		m_allTransparents.m_partInstances.clear();
		if (activeWorld)
		{
			ModelDataHandle currentMeshDataHandle;			// the current cached mesh data
			uint32_t currentMeshBaseMaterial = -1;			// mesh material base index
			uint32_t currentMeshFirstPartIndex = 0;			// mesh part data base index
			uint32_t currentMeshPartCount = 0;				// num. parts in mesh
			Entities::EntityHandle currentMaterialEntity;	// cache the current material override
			uint32_t lastMatOverrideGpuIndex = -1;			// base index of the current material override
			auto forEachEntity = [&](const Entities::EntityHandle& e, StaticMeshComponent& s, TransformComponent& t)
			{
				if (s.m_modelHandle.m_index != -1 && s.m_shouldDraw)	// doesn't mean the model is actually ready to draw!
				{
					if (s.m_modelHandle.m_index != currentMeshDataHandle.m_index)	// cache only the model data that we need
					{
						StaticMeshGpuData currentMeshData;
						if (!staticMeshes->GetMeshDataForModel(s.m_modelHandle, currentMeshData))
						{
							return true;
						}
						currentMeshDataHandle.m_index = s.m_modelHandle.m_index;
						currentMeshBaseMaterial = currentMeshData.m_materialGpuIndex;
						currentMeshFirstPartIndex = currentMeshData.m_firstMeshPartOffset;
						currentMeshPartCount = currentMeshData.m_meshPartCount;
					}
					if (s.m_materialOverride != currentMaterialEntity)		// cache the material override
					{
						currentMaterialEntity = s.m_materialOverride;
						const auto materialComponent = activeWorld->GetComponent<StaticMeshMaterialsComponent>(s.m_materialOverride);
						lastMatOverrideGpuIndex = materialComponent ? static_cast<uint32_t>(materialComponent->m_gpuDataIndex) : -1;						
					}
					const uint32_t materialBaseIndex = lastMatOverrideGpuIndex != -1 ? lastMatOverrideGpuIndex : currentMeshBaseMaterial;
					const glm::mat4 instanceTransform = t.GetWorldspaceInterpolated(e, *activeWorld);
					for (uint32_t part = 0; part < currentMeshPartCount; ++part)
					{
						const StaticMeshPart* currentPart = staticMeshes->GetMeshPart(currentMeshFirstPartIndex + part);
						const uint32_t relativePartMatIndex = currentPart->m_materialIndex - currentMeshBaseMaterial;
						const glm::mat4 partTransform = instanceTransform * currentPart->m_transform;

						// write instance data now even if it will not be used (faster than duplicating later)
						if (m_currentInstanceBufferOffset < c_maxInstances)
						{
#ifdef ENABLE_CPU_INSTANCE_CULLING
							m_globalInstancesCPU[m_currentInstanceBufferOffset].m_transform = partTransform;
							m_globalInstancesCPU[m_currentInstanceBufferOffset].m_materialIndex = materialBaseIndex + relativePartMatIndex;
#endif

							StaticMeshInstanceGpu* instancesPtr = static_cast<StaticMeshInstanceGpu*>(m_globalInstancesMappedPtr) + m_currentInstanceBufferStart + m_currentInstanceBufferOffset;
							instancesPtr->m_transform = partTransform;
							instancesPtr->m_materialIndex = materialBaseIndex + relativePartMatIndex;

							BucketPartInstance bucketInstance;
							bucketInstance.m_partGlobalIndex = currentMeshFirstPartIndex + part;
							bucketInstance.m_partInstanceIndex = m_currentInstanceBufferStart + m_currentInstanceBufferOffset;

							const StaticMeshMaterial* meshMaterial = staticMeshes->GetMeshMaterial(materialBaseIndex + relativePartMatIndex);
							if (!m_forwardRenderEverything && meshMaterial->m_albedoOpacity.w >= 1.0f)
							{
								m_allOpaques.m_partInstances.emplace_back(bucketInstance);
							}
							else
							{
								m_allTransparents.m_partInstances.emplace_back(bucketInstance);
							}

							m_currentInstanceBufferOffset++;
						}
						else
						{
							LogWarn("Max instances {} hit! Increase StaticMeshRenderer::c_maxInstances or simplify the scene!", c_maxInstances);
							return false;
						}
					}
					m_frameStats.m_totalModelInstances++;
					m_frameStats.m_totalPartInstances += currentMeshPartCount;
				}
				return true;
			};
			Entities::Queries::ForEach<StaticMeshComponent, TransformComponent>(activeWorld, forEachEntity);
		}
	}

	void StaticMeshRenderer::PrepareDrawBucket(MeshPartDrawBucket& bucket)
	{
		R3_PROF_EVENT();
		auto staticMeshes = GetSystem<StaticMeshSystem>();
		bucket.m_firstDrawOffset = (m_currentDrawBufferStart + m_currentDrawBufferOffset);
		bucket.m_drawCount = 0;
		uint64_t totalIndices = 0;
		for (const auto& bucketInstance : bucket.m_partInstances)
		{
			const StaticMeshPart* currentPartData = staticMeshes->GetMeshPart(bucketInstance.m_partGlobalIndex);
			VkDrawIndexedIndirectCommand* drawPtr = static_cast<VkDrawIndexedIndirectCommand*>(m_drawIndirectMappedPtr) + m_currentDrawBufferStart + m_currentDrawBufferOffset;
			drawPtr->indexCount = currentPartData->m_indexCount;
			drawPtr->instanceCount = 1;
			drawPtr->firstIndex = (uint32_t)currentPartData->m_indexStartOffset;
			drawPtr->vertexOffset = currentPartData->m_vertexDataOffset;
			drawPtr->firstInstance = bucketInstance.m_partInstanceIndex;
			m_currentDrawBufferOffset++;
			totalIndices += currentPartData->m_indexCount;
		}
		bucket.m_drawCount = m_currentDrawBufferStart + m_currentDrawBufferOffset - bucket.m_firstDrawOffset;
		m_frameStats.m_totalTriangles += static_cast<uint32_t>(totalIndices / 3);
	}

	void StaticMeshRenderer::PrepareAndCullDrawBucket(MeshPartDrawBucket& bucket)
	{
		R3_PROF_EVENT();
		auto staticMeshes = GetSystem<StaticMeshSystem>();
		auto mainCamera = GetSystem<CameraSystem>()->GetMainCamera();
		Frustum mainFrustum(mainCamera.ProjectionMatrix() * mainCamera.ViewMatrix());
		bucket.m_firstDrawOffset = (m_currentDrawBufferStart + m_currentDrawBufferOffset);
		bucket.m_drawCount = (uint32_t)bucket.m_partInstances.size();
		uint64_t totalIndices = 0;
#ifdef ENABLE_ASYNC_CPU_INSTANCE_CULLING
		auto forEachInstance = [&](uint32_t index)
		{
			const auto& bucketInstance = bucket.m_partInstances[index];
			const StaticMeshPart* currentPartData = staticMeshes->GetMeshPart(bucketInstance.m_partGlobalIndex);
			const StaticMeshInstanceGpu* currentInstance = m_globalInstancesCPU.data() + (bucketInstance.m_partInstanceIndex - m_currentInstanceBufferStart);

			bool isVisible = mainFrustum.IsBoxVisible(glm::vec3(currentPartData->m_boundsMin), glm::vec3(currentPartData->m_boundsMax), currentInstance->m_transform);

			VkDrawIndexedIndirectCommand* drawPtr = static_cast<VkDrawIndexedIndirectCommand*>(m_drawIndirectMappedPtr) + m_currentDrawBufferStart + m_currentDrawBufferOffset + index;
			drawPtr->indexCount = currentPartData->m_indexCount;
			drawPtr->instanceCount = isVisible ? 1 : 0;
			drawPtr->firstIndex = (uint32_t)currentPartData->m_indexStartOffset;
			drawPtr->vertexOffset = currentPartData->m_vertexDataOffset;
			drawPtr->firstInstance = bucketInstance.m_partInstanceIndex;
			totalIndices += isVisible ? currentPartData->m_indexCount : 0;
		};
		GetSystem<JobSystem>()->ForEachAsync(JobSystem::ThreadPool::FastJobs, 0, (uint32_t)bucket.m_partInstances.size(), 1, 1024, forEachInstance);
		m_currentDrawBufferOffset += bucket.m_drawCount;
#else
		for (const auto& bucketInstance : bucket.m_partInstances)
		{
			const StaticMeshPart* currentPartData = staticMeshes->GetMeshPart(bucketInstance.m_partGlobalIndex);
			const StaticMeshInstanceGpu* currentInstance = m_globalInstancesCPU.data() + (bucketInstance.m_partInstanceIndex - m_currentInstanceBufferStart);

			bool isVisible = mainFrustum.IsBoxVisible(glm::vec3(currentPartData->m_boundsMin), glm::vec3(currentPartData->m_boundsMax), currentInstance->m_transform);

			VkDrawIndexedIndirectCommand* drawPtr = static_cast<VkDrawIndexedIndirectCommand*>(m_drawIndirectMappedPtr) + m_currentDrawBufferStart + m_currentDrawBufferOffset;
			drawPtr->indexCount = currentPartData->m_indexCount;
			drawPtr->instanceCount = isVisible ? 1 : 0;
			drawPtr->firstIndex = (uint32_t)currentPartData->m_indexStartOffset;
			drawPtr->vertexOffset = currentPartData->m_vertexDataOffset;
			drawPtr->firstInstance = bucketInstance.m_partInstanceIndex;
			m_currentDrawBufferOffset++;
			totalIndices += isVisible ? currentPartData->m_indexCount : 0;
		}
#endif
		m_frameStats.m_totalTriangles += static_cast<uint32_t>(totalIndices / 3);
	}

	void StaticMeshRenderer::PrepareAndCullDrawBucketCompute(Device& d, VkCommandBuffer cmds, MeshPartDrawBucket& bucket)
	{
		R3_PROF_EVENT();

		auto mainCamera = GetSystem<CameraSystem>()->GetMainCamera();
		Frustum mainFrustum(mainCamera.ProjectionMatrix() * mainCamera.ViewMatrix());
		bucket.m_firstDrawOffset = (m_currentDrawBufferStart + m_currentDrawBufferOffset);	// record draw-indirect base offset for this bucket
		bucket.m_drawCount = (uint32_t)bucket.m_partInstances.size();						// draw-indirects will be populated by compute shader
		m_currentDrawBufferOffset += bucket.m_drawCount;

		m_computeCulling->Run(d, cmds, bucket, mainFrustum);										// populate the draw-indirect data for this bucket of instances via compute
	}

	bool StaticMeshRenderer::CollectInstances()
	{
		R3_PROF_EVENT();

		m_frameStats.m_totalTriangles = m_frameStats.m_totalModelInstances = m_frameStats.m_totalPartInstances = 0;
		m_frameStats.m_collectInstancesStartTime = GetSystem<TimeSystem>()->GetElapsedTimeReal();
		CollectAllPartInstances();
		m_frameStats.m_totalOpaqueInstances = (uint32_t)m_allOpaques.m_partInstances.size();
		m_frameStats.m_totalTransparentInstances = (uint32_t)m_allTransparents.m_partInstances.size();
		m_frameStats.m_collectInstancesEndTime = GetSystem<TimeSystem>()->GetElapsedTimeReal();

		if (m_enableComputeCulling)	// prepare buckets on cpu if no compute
		{
			m_allOpaques.m_drawCount = 0;
			m_allTransparents.m_drawCount = 0;
		}
		else
		{
			m_frameStats.m_prepareBucketsStartTime = GetSystem<TimeSystem>()->GetElapsedTimeReal();
#ifdef ENABLE_CPU_INSTANCE_CULLING
			if (m_enableCpuCulling)
			{
				PrepareAndCullDrawBucket(m_allOpaques);
				PrepareAndCullDrawBucket(m_allTransparents);

			}
			else
#endif
			{
				PrepareDrawBucket(m_allOpaques);
				PrepareDrawBucket(m_allTransparents);
			}
			m_frameStats.m_prepareBucketsEndTime = GetSystem<TimeSystem>()->GetElapsedTimeReal();
		}

		return true;
	}
}