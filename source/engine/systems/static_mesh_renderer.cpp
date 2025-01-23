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
		m_staticMeshInstances.Destroy(d);
		m_meshRenderBufferPool = nullptr;
		vkDestroyPipeline(d.GetVkDevice(), m_forwardPipeline, nullptr);
		vkDestroyPipeline(d.GetVkDevice(), m_gBufferPipeline, nullptr);
		vkDestroyPipelineLayout(d.GetVkDevice(), m_pipelineLayout, nullptr);
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
			ImGui::Checkbox("Enable Compute Culling", &m_enableComputeCulling);
			ImGui::Checkbox("Lock main frustum", &m_lockMainFrustum);
			ImGui::Checkbox("Enable static scene rebuild", &m_rebuildStaticScene);
			std::string txt = std::format("{} Model Instances Submitted", m_frameStats.m_totalModelInstances);
			ImGui::Text(txt.c_str());
			txt = std::format("    {} Total Part Instances", m_frameStats.m_totalPartInstances);
			ImGui::Text(txt.c_str());
			txt = std::format("    {} Opaque Part Instances", m_frameStats.m_totalOpaqueInstances);
			ImGui::Text(txt.c_str());
			txt = std::format("    {} Transparent Part Instances", m_frameStats.m_totalTransparentInstances);
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
#ifdef USE_LINEAR_BUFFER
			PrepareAndCullDrawBucketCompute(*ctx.m_device, ctx.m_graphicsCmds, m_staticMeshInstances.GetBufferDeviceAddress(), m_staticOpaques);
#else
			PrepareAndCullDrawBucketCompute(*ctx.m_device, ctx.m_graphicsCmds, m_staticMeshInstances.GetDataDeviceAddress(), m_staticOpaques);
#endif
			m_frameStats.m_prepareBucketsEndTime = GetSystem<TimeSystem>()->GetElapsedTimeReal();
		}
	}

	void StaticMeshRenderer::PrepareForRendering(class RenderPassContext& ctx)
	{
		R3_PROF_EVENT();
		if (!m_globalConstantsBuffer.IsCreated())
		{
			m_globalConstantsBuffer.SetDebugName("Static mesh global constants");
			if (!m_globalConstantsBuffer.Create(*ctx.m_device, c_maxBuffers, 1, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT))
			{
				LogError("Failed to create constant buffer");
				return;
			}
			m_globalConstantsBuffer.Allocate(c_maxBuffers);
		}
		if (m_drawIndirectMappedPtr == nullptr)
		{
			m_drawIndirectHostVisible = VulkanHelpers::CreateBuffer(ctx.m_device->GetVMA(),
				c_maxInstances * c_maxBuffers * sizeof(VkDrawIndexedIndirectCommand),
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
		if (m_meshRenderBufferPool == nullptr)
		{
			m_meshRenderBufferPool = std::make_unique<BufferPool>("Mesh renderer buffers");
		}
		if (!m_staticMeshInstances.IsCreated())
		{
#ifdef USE_LINEAR_BUFFER
			if (!m_staticMeshInstances.Create(*ctx.m_device, c_maxInstances, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, m_meshRenderBufferPool.get()))
#else
			if (!m_staticMeshInstances.Create(*ctx.m_device, c_maxInstances, c_maxInstances, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, m_meshRenderBufferPool.get()))
#endif
			{
				LogError("Failed to create static mesh instance buffer");
				return;
			}
#ifndef USE_LINEAR_BUFFER
			m_staticMeshInstances.Allocate(c_maxInstances);
#endif
		}

		// retire old static data buffers on scene rebuild + flush to new buffers for later
		if (m_rebuildStaticScene && m_staticOpaques.m_partInstances.size() > 0)
		{
			m_staticMeshInstances.RetirePooledBuffer(*ctx.m_device);
			m_staticMeshInstances.Flush(*ctx.m_device, ctx.m_graphicsCmds, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
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
#ifdef USE_LINEAR_BUFFER
		globals.m_instanceDataBufferAddress = m_staticMeshInstances.GetBufferDeviceAddress();	// todo, we need to pass this for each set of draw calls (probably in push constant)
#else
		globals.m_instanceDataBufferAddress = m_staticMeshInstances.GetDataDeviceAddress();
#endif
		m_globalConstantsBuffer.Write(m_thisFrameBuffer, 1, &globals);
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
		if (m_staticOpaques.m_drawCount == 0)
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
		pc.m_globalsBufferAddress = m_globalConstantsBuffer.GetDataDeviceAddress() + (m_thisFrameBuffer * sizeof(GlobalConstants));
		vkCmdPushConstants(ctx.m_graphicsCmds, m_pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pc), &pc);

		// Draw opaques
		vkCmdDrawIndexedIndirect(ctx.m_graphicsCmds, m_drawIndirectHostVisible.m_buffer, m_staticOpaques.m_firstDrawOffset * sizeof(VkDrawIndexedIndirectCommand), m_staticOpaques.m_drawCount, sizeof(VkDrawIndexedIndirectCommand));

		m_frameStats.m_writeCmdsEndTime = time->GetElapsedTimeReal();
	}

	void StaticMeshRenderer::OnForwardPassDraw(class RenderPassContext& ctx)
	{
		R3_PROF_EVENT();

		// if (m_forwardPipeline == VK_NULL_HANDLE)
		// {
		// 	auto mainColourTarget = ctx.GetResolvedTarget("MainColour");
		// 	auto mainDepthTarget = ctx.GetResolvedTarget("MainDepth");
		// 	if (!CreateForwardPipelineData(*ctx.m_device, mainColourTarget->m_info.m_format, mainDepthTarget->m_info.m_format))
		// 	{
		// 		LogError("Failed to create pipeline data for forward pass");
		// 	}
		// }
		// 
		// auto textures = GetSystem<TextureSystem>();
		// auto time = GetSystem<TimeSystem>();
		// if (m_allTransparents.m_drawCount == 0)
		// {
		// 	return;
		// }
		// 
		// m_frameStats.m_writeCmdsStartTime = time->GetElapsedTimeReal();
		// 
		// VkViewport viewport = { 0 };
		// viewport.x = 0.0f;
		// viewport.y = 0.0f;
		// viewport.width = ctx.m_renderExtents.x;
		// viewport.height = ctx.m_renderExtents.y;
		// viewport.minDepth = 0.0f;	// normalised! must be between 0 and 1
		// viewport.maxDepth = 1.0f;	// ^^
		// VkRect2D scissor = { 0 };
		// scissor.offset = { 0, 0 };
		// scissor.extent = { (uint32_t)viewport.width, (uint32_t)viewport.height };	// draw the full image
		// vkCmdBindPipeline(ctx.m_graphicsCmds, VK_PIPELINE_BIND_POINT_GRAPHICS, m_forwardPipeline);
		// vkCmdSetViewport(ctx.m_graphicsCmds, 0, 1, &viewport);
		// vkCmdSetScissor(ctx.m_graphicsCmds, 0, 1, &scissor);
		// vkCmdBindIndexBuffer(ctx.m_graphicsCmds, GetSystem<StaticMeshSystem>()->GetIndexBuffer(), 0, VK_INDEX_TYPE_UINT32);
		// VkDescriptorSet allTextures = textures->GetAllTexturesSet();
		// vkCmdBindDescriptorSets(ctx.m_graphicsCmds, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &allTextures, 0, nullptr);
		// PushConstants pc;
		// pc.m_globalsBufferAddress = m_globalConstantsBuffer.GetDataDeviceAddress() + (m_thisFrameBuffer * sizeof(GlobalConstants));
		// vkCmdPushConstants(ctx.m_graphicsCmds, m_pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pc), &pc);
		// 
		// // Draw opaques
		// vkCmdDrawIndexedIndirect(ctx.m_graphicsCmds, m_drawIndirectHostVisible.m_buffer, m_allTransparents.m_firstDrawOffset * sizeof(VkDrawIndexedIndirectCommand), m_allTransparents.m_drawCount, sizeof(VkDrawIndexedIndirectCommand));
		// 
		// m_frameStats.m_writeCmdsEndTime = time->GetElapsedTimeReal();
	}

	void StaticMeshRenderer::OnDrawEnd(class RenderPassContext& ctx)
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

	void StaticMeshRenderer::RebuildStaticScene()
	{
		R3_PROF_EVENT();
		auto staticMeshes = GetSystem<StaticMeshSystem>();
		auto activeWorld = Systems::GetSystem<Entities::EntitySystem>()->GetActiveWorld();
		m_staticOpaques.m_partInstances.clear();
		m_staticOpaques.m_drawCount = 0;
		m_staticOpaques.m_firstDrawOffset = 0;
		if (activeWorld)
		{
			ModelDataHandle currentMeshDataHandle;			// the current cached mesh
			StaticMeshGpuData currentMeshData;				// ^^
			uint32_t currentInstanceBufferOffset = 0;
			StaticMeshInstanceGpu* instanceWritePtr = m_staticMeshInstances.GetWritePtr();
			auto forEachEntity = [&](const Entities::EntityHandle& e, StaticMeshComponent& s, TransformComponent& t)
			{
				if (s.m_modelHandle.m_index != -1 && s.m_shouldDraw)	// doesn't mean the model is actually ready to draw!
				{
					if (s.m_modelHandle.m_index != currentMeshDataHandle.m_index)	// avoid getting the data for every instance, only get it when it changes
					{
						if (!staticMeshes->GetMeshDataForModel(s.m_modelHandle, currentMeshData))
						{
							return true;
						}
						currentMeshDataHandle.m_index = s.m_modelHandle.m_index;
					}
					const glm::mat4 instanceTransform = t.GetWorldspaceInterpolated(e, *activeWorld);
					for (uint32_t part = 0; part < currentMeshData.m_meshPartCount; ++part)
					{
						const StaticMeshPart* currentPart = staticMeshes->GetMeshPart(currentMeshData.m_firstMeshPartOffset + part);
						const uint32_t relativePartMatIndex = currentPart->m_materialIndex - currentMeshData.m_materialGpuIndex;
						const glm::mat4 partTransform = instanceTransform * currentPart->m_transform;

						// todo, we can most likely combine StaticMeshInstanceGpu and BucketPartInstance somehow + only write one entry per instance
						
#ifdef USE_LINEAR_BUFFER
						instanceWritePtr[currentInstanceBufferOffset].m_transform = partTransform;
						instanceWritePtr[currentInstanceBufferOffset].m_materialIndex = currentMeshData.m_materialGpuIndex + relativePartMatIndex;
#else
						StaticMeshInstanceGpu newInstance;
						newInstance.m_transform = partTransform;
						newInstance.m_materialIndex = currentMeshData.m_materialGpuIndex + relativePartMatIndex;
						m_staticMeshInstances.Write(currentInstanceBufferOffset, 1, &newInstance);
#endif
						BucketPartInstance bucketInstance;
						bucketInstance.m_partGlobalIndex = currentMeshData.m_firstMeshPartOffset + part;
						bucketInstance.m_partInstanceIndex = currentInstanceBufferOffset;

						const StaticMeshMaterial* meshMaterial = staticMeshes->GetMeshMaterial(currentMeshData.m_materialGpuIndex + relativePartMatIndex);
						if (meshMaterial->m_albedoOpacity.w >= 1.0f)
						{
							m_staticOpaques.m_partInstances.emplace_back(bucketInstance);
						}
						else
						{
							// m_allTransparents.m_partInstances.emplace_back(bucketInstance);
						}
						currentInstanceBufferOffset++;
					}
					m_frameStats.m_totalModelInstances++;
					m_frameStats.m_totalPartInstances += currentMeshData.m_meshPartCount;
				}
				return true;
			};
			Entities::Queries::ForEach<StaticMeshComponent, TransformComponent>(activeWorld, forEachEntity);
#ifdef USE_LINEAR_BUFFER
			m_staticMeshInstances.CommitWrites(currentInstanceBufferOffset);
#endif
		}
	}

	// populates draw calls for all instances in this bucket with no culling
	void StaticMeshRenderer::PrepareDrawBucket(MeshPartDrawBucket& bucket)
	{
		R3_PROF_EVENT();
		auto staticMeshes = GetSystem<StaticMeshSystem>();
		const uint32_t currentDrawBufferStart = m_thisFrameBuffer * c_maxInstances;
		bucket.m_firstDrawOffset = (currentDrawBufferStart + m_currentDrawBufferOffset);
		bucket.m_drawCount = (uint32_t)bucket.m_partInstances.size();
		for (const auto& bucketInstance : bucket.m_partInstances)
		{
			const StaticMeshPart* currentPartData = staticMeshes->GetMeshPart(bucketInstance.m_partGlobalIndex);
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
	void StaticMeshRenderer::PrepareAndCullDrawBucketCompute(Device& d, VkCommandBuffer cmds, VkDeviceAddress instanceDataBuffer, MeshPartDrawBucket& bucket)
	{
		R3_PROF_EVENT();

		Frustum mainFrustum = GetMainCameraFrustum();
		const uint32_t currentDrawBufferStart = m_thisFrameBuffer * c_maxInstances;
		bucket.m_firstDrawOffset = (currentDrawBufferStart + m_currentDrawBufferOffset);	// record draw-indirect base offset for this bucket
		bucket.m_drawCount = (uint32_t)bucket.m_partInstances.size();						// draw-indirects will be populated by compute shader
		m_currentDrawBufferOffset += bucket.m_drawCount;

		m_computeCulling->Run(d, cmds, instanceDataBuffer, m_drawIndirectBufferAddress, bucket, mainFrustum);	// populate the draw-indirect data for this bucket of instances via compute
	}

	Frustum StaticMeshRenderer::GetMainCameraFrustum()
	{
		static Frustum mainFrustum;
		if (!m_lockMainFrustum)
		{
			auto mainCamera = GetSystem<CameraSystem>()->GetMainCamera();
			mainFrustum = (mainCamera.ProjectionMatrix() * mainCamera.ViewMatrix());;
		}
		return mainFrustum; 
	}

	bool StaticMeshRenderer::CollectInstances()
	{
		R3_PROF_EVENT();

		m_frameStats.m_totalModelInstances = 0;
		m_frameStats.m_totalPartInstances = 0;
		m_frameStats.m_collectInstancesStartTime = GetSystem<TimeSystem>()->GetElapsedTimeReal();
		if (m_rebuildStaticScene)
		{
			RebuildStaticScene();
		}
		m_frameStats.m_totalOpaqueInstances = (uint32_t)m_staticOpaques.m_partInstances.size();
		m_frameStats.m_collectInstancesEndTime = GetSystem<TimeSystem>()->GetElapsedTimeReal();

		if (!m_enableComputeCulling)		// do nothing here, culling happens later
		{
			m_frameStats.m_prepareBucketsStartTime = GetSystem<TimeSystem>()->GetElapsedTimeReal();
			{
				PrepareDrawBucket(m_staticOpaques);
			}
			m_frameStats.m_prepareBucketsEndTime = GetSystem<TimeSystem>()->GetElapsedTimeReal();
		}

		return true;
	}
}