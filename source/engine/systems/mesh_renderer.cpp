#include "mesh_renderer.h"
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
#include "engine/systems/lua_system.h"
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

namespace R3
{
	// pushed once per drawing pass
	struct PushConstants 
	{
		glm::mat4 m_projViewTransform;
		glm::vec4 m_cameraWorldSpacePos;
		VkDeviceAddress m_vertexBufferAddress;
		VkDeviceAddress m_lightDataBufferAddress;
		VkDeviceAddress m_instanceDataBufferAddress;
		VkDeviceAddress m_lightTileMetadataAddress;		// only used in tiled lighting forward pass
	};

	MeshRenderer::MeshRenderer()
	{
		m_computeCulling = std::make_unique<MeshInstanceCullingCompute>();
	}

	MeshRenderer::~MeshRenderer()
	{
	}

	void MeshRenderer::RegisterTickFns()
	{
		R3_PROF_EVENT();
		RegisterTick("MeshRenderer::ShowGui", [this]() {
			return ShowGui();
		});
		RegisterTick("MeshRenderer::CollectInstances", [this]() {
			return CollectInstances();
		});
	}

	bool MeshRenderer::Init()
	{
		R3_PROF_EVENT();
		GetSystem<RenderSystem>()->m_onShutdownCbs.AddCallback([this](Device& d) {
			Cleanup(d);
		});
		m_onModelDataLoadedCbToken = GetSystem<StaticMeshSystem>()->RegisterModelReadyCallback([this](const ModelDataHandle& handle) {
			OnModelReady(handle);
		});

		// Register static scene rebuild script hook
		GetSystem<LuaSystem>()->RegisterFunction("RebuildStaticScene", [this]() {
			SetStaticsDirty();
		});

		return true;
	}

	void MeshRenderer::SetStaticsDirty()
	{
		m_staticSceneRebuildRequested = true;
	}

	void MeshRenderer::Cleanup(Device& d)
	{
		R3_PROF_EVENT();
		m_computeCulling->Cleanup(d);
		m_staticMaterialOverrides.Destroy(d);
		m_staticMeshInstances.Destroy(d);
		m_dynamicMeshInstances.Destroy(d);
		m_meshRenderBufferPool = nullptr;
		vkDestroyPipeline(d.GetVkDevice(), m_forwardPipeline, nullptr);
		vkDestroyPipeline(d.GetVkDevice(), m_forwardTiledPipeline, nullptr);
		vkDestroyPipeline(d.GetVkDevice(), m_gBufferPipeline, nullptr);
		vkDestroyPipelineLayout(d.GetVkDevice(), m_pipelineLayout, nullptr);
		if (m_drawIndirectHostVisible.m_allocation)
		{
			vmaUnmapMemory(d.GetVMA(), m_drawIndirectHostVisible.m_allocation);
			vmaDestroyBuffer(d.GetVMA(), m_drawIndirectHostVisible.m_buffer, m_drawIndirectHostVisible.m_allocation);
		}		
		Systems::GetSystem<StaticMeshSystem>()->UnregisterModelReadyCallback(m_onModelDataLoadedCbToken);
	}

	bool MeshRenderer::ShowGui()
	{
		R3_PROF_EVENT();
		auto& debugMenu = MenuBar::MainMenu().GetSubmenu("Debug");
		debugMenu.AddItem("Static Mesh Renderer", [this]() {
			m_showGui = !m_showGui;
		});
		if (m_showGui)
		{
			ImGui::Begin("Static Mesh Renderer");
			ImGui::Checkbox("Enable Compute Culling", &m_enableComputeCulling);
			if (ImGui::Button("Rebuild statics"))
			{
				SetStaticsDirty();
			}
			std::string txt = std::format("{} Total Part Instances", m_frameStats.m_totalPartInstances);
			ImGui::Text(txt.c_str());
			txt = std::format("    {} Statics / {} Dynamics", m_frameStats.m_totalStaticInstances, m_frameStats.m_totalDynamicInstances);
			ImGui::Text(txt.c_str());
			txt = std::format("    {} Opaque Part Instances", m_frameStats.m_totalOpaqueInstances);
			ImGui::Text(txt.c_str());
			txt = std::format("    {} Transparent Part Instances", m_frameStats.m_totalTransparentInstances);
			ImGui::Text(txt.c_str());
			txt = std::format("Part instances took {:.3f}ms to collect", 1000.0 * (m_frameStats.m_collectInstancesEndTime - m_frameStats.m_collectInstancesStartTime));
			ImGui::Text(txt.c_str());
			txt = std::format("Draw buckets took {:.3f}ms to prepare", 1000.0 * (m_frameStats.m_prepareBucketsEndTime - m_frameStats.m_prepareBucketsStartTime));
			ImGui::Text(txt.c_str());
			auto totalCmdBufferWriteTime = m_frameStats.m_writeGBufferCmdsEndTime - m_frameStats.m_writeGBufferCmdsStartTime;
			totalCmdBufferWriteTime += m_frameStats.m_writeForwardCmdsEndTime - m_frameStats.m_writeForwardCmdsStartTime;
			txt = std::format("Command buffer took {:.3f}ms to write", 1000.0 * totalCmdBufferWriteTime);
			ImGui::Text(txt.c_str());
			txt = std::format("    GBuffer Cmds: {:.3f}ms", 1000.0 * (m_frameStats.m_writeGBufferCmdsEndTime - m_frameStats.m_writeGBufferCmdsStartTime));
			ImGui::Text(txt.c_str());
			txt = std::format("    Forward Cmds: {:.3f}ms", 1000.0 * (m_frameStats.m_writeForwardCmdsEndTime - m_frameStats.m_writeForwardCmdsStartTime));
			ImGui::Text(txt.c_str());
			ImGui::End();
		}
		return true;
	}

	bool MeshRenderer::CreatePipelineLayout(Device& d)
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

	bool MeshRenderer::CreateGBufferPipelineData(Device& d, VkFormat positionMetalFormat, VkFormat normalRoughnessFormat, VkFormat albedoAOFormat, VkFormat mainDepthFormat)
	{
		R3_PROF_EVENT();
		std::string basePath = "shaders_spirv\\common\\";	// Load the shaders
		auto vertexShader = VulkanHelpers::LoadShaderModule(d.GetVkDevice(), basePath + "mesh_render.vert.spv");
		auto fragShader = VulkanHelpers::LoadShaderModule(d.GetVkDevice(), basePath + "mesh_render_gbuffer.frag.spv");
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

	void MeshRenderer::OnModelReady(const ModelDataHandle& handle)
	{
		SetStaticsDirty();
	}

	bool MeshRenderer::CreateForwardPipelineData(Device& d, VkFormat mainColourFormat, VkFormat mainDepthFormat)
	{
		R3_PROF_EVENT();
		std::string basePath = "shaders_spirv\\common\\";	// Load the shaders
		auto vertexShader = VulkanHelpers::LoadShaderModule(d.GetVkDevice(), basePath + "mesh_render.vert.spv");
		auto fragShader = VulkanHelpers::LoadShaderModule(d.GetVkDevice(), basePath + "mesh_render_forward.frag.spv");
		auto fragShaderTiled = VulkanHelpers::LoadShaderModule(d.GetVkDevice(), basePath + "mesh_render_forward_tiled.frag.spv");
		if (vertexShader == VK_NULL_HANDLE || fragShader == VK_NULL_HANDLE || fragShaderTiled == VK_NULL_HANDLE)
		{
			LogError("Failed to create shader modules");
			return false;
		}
		PipelineBuilder pb;	// Make the pipeline
		pb.m_shaderStages = {
			VulkanHelpers::CreatePipelineShaderState(VK_SHADER_STAGE_VERTEX_BIT, vertexShader),
			VulkanHelpers::CreatePipelineShaderState(VK_SHADER_STAGE_FRAGMENT_BIT, fragShader)
		};
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

		pb.m_shaderStages = {
			VulkanHelpers::CreatePipelineShaderState(VK_SHADER_STAGE_VERTEX_BIT, vertexShader),
			VulkanHelpers::CreatePipelineShaderState(VK_SHADER_STAGE_FRAGMENT_BIT, fragShaderTiled)
		};
		m_forwardTiledPipeline = pb.Build(d.GetVkDevice(), m_pipelineLayout, 1, &mainColourFormat, mainDepthFormat);
		if (m_forwardTiledPipeline == VK_NULL_HANDLE)
		{
			LogError("Failed to create pipeline!");
			return false;
		}

		vkDestroyShaderModule(d.GetVkDevice(), vertexShader, nullptr);
		vkDestroyShaderModule(d.GetVkDevice(), fragShader, nullptr);
		vkDestroyShaderModule(d.GetVkDevice(), fragShaderTiled, nullptr);
		return true;
	}

	void MeshRenderer::CullInstancesOnGpu(RenderPassContext& ctx)
	{
		R3_PROF_EVENT();
		if (m_enableComputeCulling)
		{
			m_frameStats.m_prepareBucketsStartTime = GetSystem<TimeSystem>()->GetElapsedTimeReal();
			PrepareAndCullDrawBucketCompute(*ctx.m_device, ctx.m_graphicsCmds, m_staticMeshInstances.GetBufferDeviceAddress(), m_staticOpaques, m_staticOpaqueDrawData);
			PrepareAndCullDrawBucketCompute(*ctx.m_device, ctx.m_graphicsCmds, m_staticMeshInstances.GetBufferDeviceAddress(), m_staticTransparents, m_staticTransparentDrawData);
			PrepareAndCullDrawBucketCompute(*ctx.m_device, ctx.m_graphicsCmds, m_dynamicMeshInstances.GetBufferDeviceAddress(), m_dynamicOpaques, m_dynamicOpaqueDrawData);
			PrepareAndCullDrawBucketCompute(*ctx.m_device, ctx.m_graphicsCmds, m_dynamicMeshInstances.GetBufferDeviceAddress(), m_dynamicTransparents, m_dynamicTransparentDrawData);
			m_frameStats.m_prepareBucketsEndTime = GetSystem<TimeSystem>()->GetElapsedTimeReal();
		}
	}

	void MeshRenderer::PrepareForRendering(class RenderPassContext& ctx)
	{
		R3_PROF_EVENT();
		if (m_drawIndirectMappedPtr == nullptr)
		{
			m_drawIndirectHostVisible = VulkanHelpers::CreateBuffer(ctx.m_device->GetVMA(),
				c_maxInstances * c_maxBuffers * sizeof(VkDrawIndexedIndirectCommand),
				VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VMA_MEMORY_USAGE_AUTO, VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);
			VulkanHelpers::SetBufferName(ctx.m_device->GetVkDevice(), m_drawIndirectHostVisible, "Static mesh draw indirect");
			vmaMapMemory(ctx.m_device->GetVMA(), m_drawIndirectHostVisible.m_allocation, &m_drawIndirectMappedPtr);
			m_drawIndirectBufferAddress = VulkanHelpers::GetBufferDeviceAddress(ctx.m_device->GetVkDevice(), m_drawIndirectHostVisible);
		}
		if (m_pipelineLayout == VK_NULL_HANDLE)
		{
			if (!CreatePipelineLayout(*ctx.m_device))
			{
				LogError("Failed to create static mesh pipeline layout");
				return;
			}
		}
		if (m_meshRenderBufferPool == nullptr)
		{
			m_meshRenderBufferPool = std::make_unique<BufferPool>("Mesh renderer buffers");
		}
		if (!m_staticMeshInstances.IsCreated())
		{
			m_staticMeshInstances.SetDebugName("Static mesh instances");
			if (!m_staticMeshInstances.Create(*ctx.m_device, c_maxInstances, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, m_meshRenderBufferPool.get()))
			{
				LogError("Failed to create static mesh instance buffer");
				return;
			}
		}
		if (!m_staticMaterialOverrides.IsCreated())
		{
			m_staticMeshInstances.SetDebugName("Static material overrides");
			if (!m_staticMaterialOverrides.Create(*ctx.m_device, c_maxStaticMaterialOverrides, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, m_meshRenderBufferPool.get()))
			{
				LogError("Failed to create static mesh instance buffer");
				return;
			}
		}
		if (!m_dynamicMeshInstances.IsCreated())
		{
			m_dynamicMeshInstances.SetDebugName("Dynamic mesh instances");
			if (!m_dynamicMeshInstances.Create(*ctx.m_device, c_maxInstances, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, m_meshRenderBufferPool.get()))
			{
				LogError("Failed to create dynamic mesh instance buffer");
				return;
			}
		}

		// old static data buffers have been retired, flush writes to the new ones
		if (m_rebuildingStaticScene)
		{
			m_staticMaterialOverrides.Flush(*ctx.m_device, ctx.m_graphicsCmds, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
			if ((m_staticOpaques.m_partInstances.size() > 0 || m_staticTransparents.m_partInstances.size() > 0))
			{
				m_staticMeshInstances.Flush(*ctx.m_device, ctx.m_graphicsCmds, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
			}
			m_rebuildingStaticScene = false;
		}

		// Flush dynamic data every frame
		if ((m_dynamicOpaques.m_partInstances.size() > 0 || m_dynamicTransparents.m_partInstances.size() > 0))
		{
			m_dynamicMeshInstances.Flush(*ctx.m_device, ctx.m_graphicsCmds, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
		}
	}

	void MeshRenderer::OnGBufferPassDraw(class RenderPassContext& ctx)
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

		if (m_staticOpaqueDrawData.m_drawCount == 0 && m_dynamicOpaqueDrawData.m_drawCount == 0)
		{
			return;
		}

		auto textures = GetSystem<TextureSystem>();
		auto time = GetSystem<TimeSystem>();
		auto cameras = GetSystem<CameraSystem>();
		auto staticMeshes = GetSystem<StaticMeshSystem>();

		m_frameStats.m_writeGBufferCmdsStartTime = time->GetElapsedTimeReal();

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
		pc.m_cameraWorldSpacePos = glm::vec4(cameras->GetMainCamera().Position(), 1);
		pc.m_projViewTransform = cameras->GetMainCamera().ProjectionMatrix() * cameras->GetMainCamera().ViewMatrix();
		pc.m_vertexBufferAddress = staticMeshes->GetVertexDataDeviceAddress();
		pc.m_instanceDataBufferAddress = m_staticMeshInstances.GetBufferDeviceAddress();
		pc.m_lightDataBufferAddress = 0;	// light data unnused in gbuffer pass
		pc.m_lightTileMetadataAddress = 0;

		vkCmdPushConstants(ctx.m_graphicsCmds, m_pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pc), &pc);
		vkCmdDrawIndexedIndirect(ctx.m_graphicsCmds, m_drawIndirectHostVisible.m_buffer, m_staticOpaqueDrawData.m_firstDrawOffset * sizeof(VkDrawIndexedIndirectCommand), m_staticOpaqueDrawData.m_drawCount, sizeof(VkDrawIndexedIndirectCommand));

		pc.m_instanceDataBufferAddress = m_dynamicMeshInstances.GetBufferDeviceAddress();
		vkCmdPushConstants(ctx.m_graphicsCmds, m_pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pc), &pc);
		vkCmdDrawIndexedIndirect(ctx.m_graphicsCmds, m_drawIndirectHostVisible.m_buffer, m_dynamicOpaqueDrawData.m_firstDrawOffset * sizeof(VkDrawIndexedIndirectCommand), m_dynamicOpaqueDrawData.m_drawCount, sizeof(VkDrawIndexedIndirectCommand));

		m_frameStats.m_writeGBufferCmdsEndTime = time->GetElapsedTimeReal();
	}

	void MeshRenderer::OnForwardPassDraw(class RenderPassContext& ctx, bool useTiledLighting)
	{
		R3_PROF_EVENT();

		if (m_forwardPipeline == VK_NULL_HANDLE || m_forwardTiledPipeline == VK_NULL_HANDLE)
		{
			auto mainColourTarget = ctx.GetResolvedTarget("MainColour");
			auto mainDepthTarget = ctx.GetResolvedTarget("MainDepth");
			if (!CreateForwardPipelineData(*ctx.m_device, mainColourTarget->m_info.m_format, mainDepthTarget->m_info.m_format))
			{
				LogError("Failed to create pipeline data for forward pass");
			}
		}
		
		if (m_staticTransparentDrawData.m_drawCount == 0 && m_dynamicTransparentDrawData.m_drawCount == 0)
		{
			return;
		}

		auto textures = GetSystem<TextureSystem>();
		auto time = GetSystem<TimeSystem>();
		auto cameras = GetSystem<CameraSystem>();
		auto staticMeshes = GetSystem<StaticMeshSystem>();
		auto lights = GetSystem<LightsSystem>();
		
		m_frameStats.m_writeForwardCmdsStartTime = time->GetElapsedTimeReal();
		
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
		if (useTiledLighting && m_lightTileMetadata != 0)
		{
			vkCmdBindPipeline(ctx.m_graphicsCmds, VK_PIPELINE_BIND_POINT_GRAPHICS, m_forwardTiledPipeline);
		}
		else
		{
			vkCmdBindPipeline(ctx.m_graphicsCmds, VK_PIPELINE_BIND_POINT_GRAPHICS, m_forwardPipeline);
		}
		vkCmdSetViewport(ctx.m_graphicsCmds, 0, 1, &viewport);
		vkCmdSetScissor(ctx.m_graphicsCmds, 0, 1, &scissor);
		vkCmdBindIndexBuffer(ctx.m_graphicsCmds, GetSystem<StaticMeshSystem>()->GetIndexBuffer(), 0, VK_INDEX_TYPE_UINT32);
		VkDescriptorSet allTextures = textures->GetAllTexturesSet();
		vkCmdBindDescriptorSets(ctx.m_graphicsCmds, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &allTextures, 0, nullptr);

		PushConstants pc;
		pc.m_cameraWorldSpacePos = glm::vec4(cameras->GetMainCamera().Position(), 1);
		pc.m_projViewTransform = cameras->GetMainCamera().ProjectionMatrix() * cameras->GetMainCamera().ViewMatrix();
		pc.m_vertexBufferAddress = staticMeshes->GetVertexDataDeviceAddress();
		pc.m_instanceDataBufferAddress = m_staticMeshInstances.GetBufferDeviceAddress();
		pc.m_lightDataBufferAddress = lights->GetAllLightsDeviceAddress();
		pc.m_lightTileMetadataAddress = m_lightTileMetadata;				// should come from lights system/param, eventually there will be multiple

		vkCmdPushConstants(ctx.m_graphicsCmds, m_pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pc), &pc);
		vkCmdDrawIndexedIndirect(ctx.m_graphicsCmds, m_drawIndirectHostVisible.m_buffer, m_staticTransparentDrawData.m_firstDrawOffset * sizeof(VkDrawIndexedIndirectCommand), m_staticTransparentDrawData.m_drawCount, sizeof(VkDrawIndexedIndirectCommand));
		
		pc.m_instanceDataBufferAddress = m_dynamicMeshInstances.GetBufferDeviceAddress();
		vkCmdPushConstants(ctx.m_graphicsCmds, m_pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pc), &pc);
		vkCmdDrawIndexedIndirect(ctx.m_graphicsCmds, m_drawIndirectHostVisible.m_buffer, m_dynamicTransparentDrawData.m_firstDrawOffset * sizeof(VkDrawIndexedIndirectCommand), m_dynamicTransparentDrawData.m_drawCount, sizeof(VkDrawIndexedIndirectCommand));


		m_frameStats.m_writeForwardCmdsEndTime = time->GetElapsedTimeReal();
	}

	void MeshRenderer::OnDrawEnd(class RenderPassContext& ctx)
	{
		R3_PROF_EVENT();

		// Update draw buffers for next frame
		if (++m_thisFrameBuffer >= c_maxBuffers)
		{
			m_thisFrameBuffer = 0;
		}

		// reset draw offset for next frame
		m_currentDrawBufferOffset = 0;

		m_computeCulling->Reset();
	}

	void MeshRenderer::RebuildStaticMaterialOverrides()
	{
		R3_PROF_EVENT();
		auto activeWorld = GetSystem<Entities::EntitySystem>()->GetActiveWorld();
		if (activeWorld)
		{
			MeshMaterial* materialWritePtr = m_staticMaterialOverrides.GetWritePtr();
			uint32_t currentMaterialIndex = 0;
			auto forEachEntity = [&](const Entities::EntityHandle& e, StaticMeshMaterialsComponent& cmp)
			{
				cmp.m_gpuDataIndex = currentMaterialIndex;	// assign new index into static material buffer
				for (uint32_t m = 0; m < cmp.m_materials.size(); ++m)
				{
					materialWritePtr[currentMaterialIndex++] = cmp.m_materials[m];	// upload all materials
				}
				return true;
			};
			Entities::Queries::ForEach<StaticMeshMaterialsComponent>(activeWorld, forEachEntity);
			m_staticMaterialOverrides.CommitWrites(currentMaterialIndex);
			m_staticMaterialOverrides.RetirePooledBuffer(*GetSystem<RenderSystem>()->GetDevice());	// retire the old buffer here so RebuildStaticInstances gets the correct buffer address for the new one
		}
	}

	template<class MeshCmpType, bool UseInterpolatedTransforms>
	void MeshRenderer::RebuildInstances(LinearWriteOnlyGpuArray<MeshInstance>& instanceBuffer, MeshPartInstanceBucket& opaques, MeshPartInstanceBucket& transparents)
	{
		R3_PROF_EVENT();
		auto staticMeshes = GetSystem<StaticMeshSystem>();
		auto activeWorld = GetSystem<Entities::EntitySystem>()->GetActiveWorld();
		if (activeWorld)
		{
			ModelDataHandle currentMeshDataHandle;			// the current cached mesh
			MeshDrawData currentMeshData;					// ^^
			Entities::EntityHandle currentMaterialEntity;	// the current cached material override
			uint32_t lastMatOverrideGpuIndex = -1;			// base index of the current material override
			const MeshMaterial* overrideMaterials = nullptr;	// cache a ptr to the last override components' material data
			uint32_t currentInstanceBufferOffset = 0;
			MeshInstance* instanceWritePtr = instanceBuffer.GetWritePtr();
			auto forEachEntity = [&](const Entities::EntityHandle& e, MeshCmpType& s, TransformComponent& t)
			{
				const auto modelHandle = s.GetModelHandle();
				if (modelHandle.m_index != -1 && s.GetShouldDraw())	// doesn't mean the model is actually ready to draw!
				{
					if (modelHandle.m_index != currentMeshDataHandle.m_index)	// avoid getting the data for every instance, only get it when it changes
					{
						if (!staticMeshes->GetMeshDataForModel(modelHandle, currentMeshData))
						{
							return true;
						}
						currentMeshDataHandle.m_index = modelHandle.m_index;
					}
					if (s.GetMaterialOverride() != currentMaterialEntity)		// cache the material override
					{
						currentMaterialEntity = s.GetMaterialOverride();
						const auto materialComponent = activeWorld->GetComponent<StaticMeshMaterialsComponent>(s.GetMaterialOverride());
						const bool overrideValid = materialComponent && materialComponent->m_materials.size() >= currentMeshData.m_materialCount;
						lastMatOverrideGpuIndex = overrideValid ? static_cast<uint32_t>(materialComponent->m_gpuDataIndex) : -1;
						overrideMaterials = overrideValid ? materialComponent->m_materials.data() : nullptr;
					}
					VkDeviceAddress materialBaseAddress = lastMatOverrideGpuIndex == -1 ?
						staticMeshes->GetMaterialsDeviceAddress() + (currentMeshData.m_materialGpuIndex * sizeof(MeshMaterial)) :
						m_staticMaterialOverrides.GetBufferDeviceAddress() + (lastMatOverrideGpuIndex * sizeof(MeshMaterial));
					glm::mat4 instanceTransform;
					if constexpr (UseInterpolatedTransforms)
					{
						instanceTransform = t.GetWorldspaceInterpolated(e, *activeWorld);
					}
					else
					{
						instanceTransform = t.GetWorldspaceMatrix(e, *activeWorld);
					}
					for (uint32_t part = 0; part < currentMeshData.m_meshPartCount; ++part)
					{
						const MeshPart* currentPart = staticMeshes->GetMeshPart(currentMeshData.m_firstMeshPartOffset + part);
						const uint32_t relativePartMatIndex = currentPart->m_materialIndex - currentMeshData.m_materialGpuIndex;
						const glm::mat4 partTransform = instanceTransform * currentPart->m_transform;

						VkDeviceAddress materialAddress = materialBaseAddress + (relativePartMatIndex * sizeof(MeshMaterial));
						instanceWritePtr[currentInstanceBufferOffset].m_transform = partTransform;
						instanceWritePtr[currentInstanceBufferOffset].m_materialDataAddress = materialAddress;

						BucketPartInstance bucketInstance;
						bucketInstance.m_partGlobalIndex = currentMeshData.m_firstMeshPartOffset + part;
						bucketInstance.m_partInstanceIndex = currentInstanceBufferOffset;

						const MeshMaterial* meshMaterial = overrideMaterials == nullptr ?
							staticMeshes->GetMeshMaterial(currentMeshData.m_materialGpuIndex + relativePartMatIndex) : &overrideMaterials[relativePartMatIndex];
						if (meshMaterial->m_albedoOpacity.w >= 1.0f)
						{
							opaques.m_partInstances.emplace_back(bucketInstance);
						}
						else
						{
							transparents.m_partInstances.emplace_back(bucketInstance);
						}
						currentInstanceBufferOffset++;
					}
				}
				return true;
			};
			Entities::Queries::ForEach<MeshCmpType, TransformComponent>(activeWorld, forEachEntity);
			if (currentInstanceBufferOffset > 0)
			{
				instanceBuffer.CommitWrites(currentInstanceBufferOffset);
				instanceBuffer.RetirePooledBuffer(*GetSystem<RenderSystem>()->GetDevice());		// write to a new buffer each time
			}
		}
	}

	void MeshRenderer::RebuildStaticScene()
	{
		R3_PROF_EVENT();
		m_staticOpaques.m_partInstances.clear();
		m_staticOpaqueDrawData.m_drawCount = 0;
		m_staticOpaqueDrawData.m_firstDrawOffset = 0;
		m_staticTransparents.m_partInstances.clear();
		m_staticTransparentDrawData.m_drawCount = 0;
		m_staticTransparentDrawData.m_firstDrawOffset = 0;
		RebuildStaticMaterialOverrides();
		RebuildInstances<StaticMeshComponent, false>(m_staticMeshInstances, m_staticOpaques, m_staticTransparents);
	}

	// must be called after RebuildStaticScene to get proper material updates after scene rebuild
	void MeshRenderer::RebuildDynamicScene()
	{
		m_dynamicOpaques.m_partInstances.clear();
		m_dynamicOpaqueDrawData.m_drawCount = 0;
		m_dynamicOpaqueDrawData.m_firstDrawOffset = 0;
		m_dynamicTransparents.m_partInstances.clear();
		m_dynamicTransparentDrawData.m_drawCount = 0;
		m_dynamicTransparentDrawData.m_firstDrawOffset = 0;
		RebuildInstances<DynamicMeshComponent, true>(m_dynamicMeshInstances, m_dynamicOpaques, m_dynamicTransparents);
	}

	// populates draw calls for all instances in this bucket with no culling on cpu
	void MeshRenderer::PrepareDrawBucket(const MeshPartInstanceBucket& bucket, MeshPartBucketDrawIndirects& drawData)
	{
		R3_PROF_EVENT();
		auto staticMeshes = GetSystem<StaticMeshSystem>();
		const uint32_t currentDrawBufferStart = m_thisFrameBuffer * c_maxInstances;
		drawData.m_firstDrawOffset = (currentDrawBufferStart + m_currentDrawBufferOffset);
		drawData.m_drawCount = (uint32_t)bucket.m_partInstances.size();
		for (const auto& bucketInstance : bucket.m_partInstances)
		{
			const MeshPart* currentPartData = staticMeshes->GetMeshPart(bucketInstance.m_partGlobalIndex);
			VkDrawIndexedIndirectCommand* drawPtr = static_cast<VkDrawIndexedIndirectCommand*>(m_drawIndirectMappedPtr) + currentDrawBufferStart + m_currentDrawBufferOffset;
			drawPtr->indexCount = currentPartData->m_indexCount;
			drawPtr->instanceCount = 1;
			drawPtr->firstIndex = (uint32_t)currentPartData->m_indexStartOffset;
			drawPtr->vertexOffset = currentPartData->m_vertexDataOffset;
			drawPtr->firstInstance = bucketInstance.m_partInstanceIndex;
			m_currentDrawBufferOffset++;
		}
	}

	// use compute to cull and prepare draw calls for instances in this bucket
	void MeshRenderer::PrepareAndCullDrawBucketCompute(Device& d, VkCommandBuffer cmds, VkDeviceAddress instanceDataBuffer, const MeshPartInstanceBucket& bucket, MeshPartBucketDrawIndirects& drawData)
	{
		R3_PROF_EVENT();

		Frustum mainFrustum = GetMainCameraFrustum();
		const uint32_t currentDrawBufferStart = m_thisFrameBuffer * c_maxInstances;
		drawData.m_firstDrawOffset = (currentDrawBufferStart + m_currentDrawBufferOffset);	// record draw-indirect base offset for this bucket
		drawData.m_drawCount = (uint32_t)bucket.m_partInstances.size();						// draw-indirects will be populated by compute shader
		m_currentDrawBufferOffset += drawData.m_drawCount;

		m_computeCulling->Run(d, cmds, instanceDataBuffer, m_drawIndirectBufferAddress, bucket, drawData, mainFrustum);	// populate the draw-indirect data for this bucket of instances via compute
	}

	Frustum MeshRenderer::GetMainCameraFrustum()
	{
		auto mainCamera = GetSystem<CameraSystem>()->GetMainCamera();
		return Frustum(mainCamera.ProjectionMatrix() * mainCamera.ViewMatrix());
	}

	bool MeshRenderer::CollectInstances()
	{
		R3_PROF_EVENT();

		// this is the safest place to trigger static scene rebuild
		if (m_staticSceneRebuildRequested.exchange(false) == true)
		{
			m_rebuildingStaticScene = true;
		}

		m_frameStats.m_collectInstancesStartTime = GetSystem<TimeSystem>()->GetElapsedTimeReal();
		if (m_rebuildingStaticScene)
		{
			RebuildStaticScene();
		}
		RebuildDynamicScene();

		m_frameStats.m_totalOpaqueInstances = (uint32_t)(m_staticOpaques.m_partInstances.size() + m_dynamicOpaques.m_partInstances.size());
		m_frameStats.m_totalTransparentInstances = (uint32_t)(m_staticTransparents.m_partInstances.size() + m_dynamicTransparents.m_partInstances.size());
		m_frameStats.m_totalPartInstances = m_frameStats.m_totalOpaqueInstances + m_frameStats.m_totalTransparentInstances;
		m_frameStats.m_collectInstancesEndTime = GetSystem<TimeSystem>()->GetElapsedTimeReal();
		m_frameStats.m_totalStaticInstances = (uint32_t)(m_staticOpaques.m_partInstances.size() + m_staticTransparents.m_partInstances.size());
		m_frameStats.m_totalDynamicInstances = (uint32_t)(m_dynamicOpaques.m_partInstances.size() + m_dynamicTransparents.m_partInstances.size());

		if (!m_enableComputeCulling)		// do nothing here, gpu culling happens later
		{
			m_frameStats.m_prepareBucketsStartTime = GetSystem<TimeSystem>()->GetElapsedTimeReal();
			{
				PrepareDrawBucket(m_staticOpaques, m_staticOpaqueDrawData);
				PrepareDrawBucket(m_staticTransparents, m_staticTransparentDrawData);
				PrepareDrawBucket(m_dynamicOpaques, m_dynamicOpaqueDrawData);
				PrepareDrawBucket(m_dynamicTransparents, m_dynamicTransparentDrawData);
			}
			m_frameStats.m_prepareBucketsEndTime = GetSystem<TimeSystem>()->GetElapsedTimeReal();
		}

		return true;
	}
}