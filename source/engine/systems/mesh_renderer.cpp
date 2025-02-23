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
#include "engine/systems/immediate_render_system.h"
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
	struct MeshRenderer::ShaderGlobals
	{
		glm::mat4 m_projViewTransform;
		glm::mat4 m_worldToViewTransform;
		glm::vec4 m_cameraWorldSpacePos;				// w unused
		VkDeviceAddress m_vertexPosUVBufferAddress;
		VkDeviceAddress m_verteNormTangentBufferAddress;
		VkDeviceAddress m_lightDataBufferAddress;
		VkDeviceAddress m_lightTileMetadataAddress;		// only used in tiled lighting forward pass
	};

	// pushed once per drawing pass
	struct PushConstants 
	{
		VkDeviceAddress m_globalsBuffer;				// globals address for this pass
		VkDeviceAddress m_instanceDataBufferAddress;	// instances for this set of draws
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
		m_globalsBuffer.Destroy(d);
		vkDestroyPipeline(d.GetVkDevice(), m_forwardPipeline, nullptr);
		vkDestroyPipeline(d.GetVkDevice(), m_forwardTiledPipeline, nullptr);
		vkDestroyPipeline(d.GetVkDevice(), m_gBufferPipeline, nullptr);
		vkDestroyPipeline(d.GetVkDevice(), m_shadowPipeline, nullptr);
		vkDestroyPipelineLayout(d.GetVkDevice(), m_pipelineLayout, nullptr);
		vkDestroyPipelineLayout(d.GetVkDevice(), m_pipelineLayoutWithShadowmaps, nullptr);
		GetSystem<RenderSystem>()->GetBufferPool()->Release(m_drawIndirectHostVisible);
		Systems::GetSystem<StaticMeshSystem>()->UnregisterModelReadyCallback(m_onModelDataLoadedCbToken);
	}

	void MeshRenderer::ShowPerfStatsGui()
	{
		std::string txt = std::format("{} Total Part Instances", m_frameStats.m_totalPartInstances);
		ImGui::Text(txt.c_str());
		txt = std::format("    {} Statics / {} Dynamics", m_frameStats.m_totalStaticInstances, m_frameStats.m_totalDynamicInstances);
		ImGui::Text(txt.c_str());
		txt = std::format("    {} Opaque Part Instances", m_frameStats.m_totalOpaqueInstances);
		ImGui::Text(txt.c_str());
		txt = std::format("    {} Transparent Part Instances", m_frameStats.m_totalTransparentInstances);
		ImGui::Text(txt.c_str());
		txt = std::format("    {} Static Shadow Casters", m_frameStats.m_totalStaticShadowCasters);
		ImGui::Text(txt.c_str());
		txt = std::format("    {} Dynamic Shadow Casters", m_frameStats.m_totalDynamicShadowCasters);
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
	}

	bool MeshRenderer::ShowGui()
	{
		R3_PROF_EVENT();
		auto& debugMenu = MenuBar::MainMenu().GetSubmenu("Debug");
		debugMenu.AddItem("Mesh Renderer Settings", [this]() {
			m_showGui = !m_showGui;
		});
		if (m_showGui)
		{
			ImGui::Begin("Mesh Renderer");
			ImGui::Checkbox("Enable Compute Culling", &m_enableComputeCulling);
			ImGui::Checkbox("Enable Shadow Caster Culling", &m_enableLightCascadeCulling);
			if (ImGui::Button("Rebuild statics"))
			{
				SetStaticsDirty();
			}
			ImGui::End();
		}
		return true;
	}

	bool MeshRenderer::CreatePipelineLayouts(Device& d)
	{
		R3_PROF_EVENT();
		auto textures = GetSystem<TextureSystem>();
		auto lights = GetSystem<LightsSystem>();
		VkPushConstantRange constantRange;	// Create pipeline layout
		constantRange.offset = 0;	// needs to match in the shader if >0!
		constantRange.size = sizeof(PushConstants);
		constantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
		VkPipelineLayoutCreateInfo pipelineLayoutInfo = { 0 };
		pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutInfo.pushConstantRangeCount = 1;
		pipelineLayoutInfo.pPushConstantRanges = &constantRange;
		VkDescriptorSetLayout setLayouts[] = { textures->GetDescriptorsLayout(), lights->GetShadowMapDescriptorLayout()};
		pipelineLayoutInfo.pSetLayouts = setLayouts;
		pipelineLayoutInfo.setLayoutCount = 1;	// textures only
		if (!VulkanHelpers::CheckResult(vkCreatePipelineLayout(d.GetVkDevice(), &pipelineLayoutInfo, nullptr, &m_pipelineLayout)))
		{
			LogError("Failed to create pipeline layout");
			return false;
		}

		pipelineLayoutInfo.setLayoutCount = 2;	// textures + shadow maps
		if (!VulkanHelpers::CheckResult(vkCreatePipelineLayout(d.GetVkDevice(), &pipelineLayoutInfo, nullptr, &m_pipelineLayoutWithShadowmaps)))
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

	bool MeshRenderer::CreateShadowPipelineData(Device& d, VkFormat depthBuferFormat)
	{
		R3_PROF_EVENT();
		std::string basePath = "shaders_spirv\\common\\";	// Load the shaders
		auto vertexShader = VulkanHelpers::LoadShaderModule(d.GetVkDevice(), basePath + "mesh_render_shadow.vert.spv");
		auto fragShader = VulkanHelpers::LoadShaderModule(d.GetVkDevice(), basePath + "mesh_render_shadow.frag.spv");
		if (vertexShader == VK_NULL_HANDLE || fragShader == VK_NULL_HANDLE)
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
			VK_DYNAMIC_STATE_SCISSOR,
			VK_DYNAMIC_STATE_DEPTH_BIAS	// we need to set this per cascade, but the enable flag can be set for the pipeline itself
		};
		pb.m_dynamicState = VulkanHelpers::CreatePipelineDynamicState(dynamicStates);
		pb.m_vertexInputState = VulkanHelpers::CreatePipelineEmptyVertexInputState();
		pb.m_inputAssemblyState = VulkanHelpers::CreatePipelineInputAssemblyState(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
		pb.m_viewportState = VulkanHelpers::CreatePipelineDynamicViewportState();
		pb.m_rasterState = VulkanHelpers::CreatePipelineRasterState(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE);

		pb.m_rasterState.depthClampEnable = VK_TRUE;	// disable depth clipping, instead clamp depth values to near/far plane
		VkPipelineRasterizationDepthClipStateCreateInfoEXT disableDepthClipping = { 0 };
		disableDepthClipping.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_DEPTH_CLIP_STATE_CREATE_INFO_EXT;
		disableDepthClipping.depthClipEnable = false;
		pb.m_rasterState.pNext = &disableDepthClipping;
		pb.m_rasterState.depthBiasEnable = VK_TRUE;		// always enable depth bias

		pb.m_multisamplingState = VulkanHelpers::CreatePipelineMultiSampleState_SingleSample();
		VkPipelineDepthStencilStateCreateInfo depthStencilState = { 0 };	// Enable depth read/write
		depthStencilState.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		depthStencilState.depthTestEnable = VK_TRUE;
		depthStencilState.depthWriteEnable = VK_TRUE;
		depthStencilState.depthCompareOp = VK_COMPARE_OP_LESS;
		depthStencilState.depthBoundsTestEnable = VK_FALSE;
		depthStencilState.stencilTestEnable = VK_FALSE;
		pb.m_depthStencilState = depthStencilState;

		VkPipelineColorBlendStateCreateInfo blending = { 0 };	// no colour attachments
		blending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		pb.m_colourBlendState = blending;

		m_shadowPipeline = pb.Build(d.GetVkDevice(), m_pipelineLayout, 0, nullptr, depthBuferFormat);
		if (m_shadowPipeline == VK_NULL_HANDLE)
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
		m_forwardPipeline = pb.Build(d.GetVkDevice(), m_pipelineLayoutWithShadowmaps, 1, &mainColourFormat, mainDepthFormat);
		if (m_forwardPipeline == VK_NULL_HANDLE)
		{
			LogError("Failed to create pipeline!");
			return false;
		}

		pb.m_shaderStages = {
			VulkanHelpers::CreatePipelineShaderState(VK_SHADER_STAGE_VERTEX_BIT, vertexShader),
			VulkanHelpers::CreatePipelineShaderState(VK_SHADER_STAGE_FRAGMENT_BIT, fragShaderTiled)
		};
		m_forwardTiledPipeline = pb.Build(d.GetVkDevice(), m_pipelineLayoutWithShadowmaps, 1, &mainColourFormat, mainDepthFormat);
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
			Frustum mainFrustum = GetMainCameraFrustum();
			m_frameStats.m_prepareBucketsStartTime = GetSystem<TimeSystem>()->GetElapsedTimeReal();
			PrepareAndCullDrawBucketCompute(*ctx.m_device, ctx.m_graphicsCmds, mainFrustum, m_staticMeshInstances.GetBufferDeviceAddress(), m_staticOpaques, m_staticOpaqueDrawData);
			PrepareAndCullDrawBucketCompute(*ctx.m_device, ctx.m_graphicsCmds, mainFrustum, m_staticMeshInstances.GetBufferDeviceAddress(), m_staticTransparents, m_staticTransparentDrawData);
			PrepareAndCullDrawBucketCompute(*ctx.m_device, ctx.m_graphicsCmds, mainFrustum, m_dynamicMeshInstances.GetBufferDeviceAddress(), m_dynamicOpaques, m_dynamicOpaqueDrawData);
			PrepareAndCullDrawBucketCompute(*ctx.m_device, ctx.m_graphicsCmds, mainFrustum, m_dynamicMeshInstances.GetBufferDeviceAddress(), m_dynamicTransparents, m_dynamicTransparentDrawData);
			if (m_enableLightCascadeCulling)
			{
				auto lights = GetSystem<LightsSystem>();
				const int cascades = lights->GetShadowCascadeCount();
				for (int i = 0; i < cascades; ++i)
				{
					Frustum sunShadowFrustum(lights->GetShadowCascadeMatrix(i));
					PrepareAndCullDrawBucketCompute(*ctx.m_device, ctx.m_graphicsCmds, sunShadowFrustum, m_staticMeshInstances.GetBufferDeviceAddress(), m_staticShadowCasters, m_staticSunShadowCastersDrawData[i]);
					PrepareAndCullDrawBucketCompute(*ctx.m_device, ctx.m_graphicsCmds, sunShadowFrustum, m_dynamicMeshInstances.GetBufferDeviceAddress(), m_dynamicShadowCasters, m_dynamicSunShadowCastersDrawData[i]);
				}
			}
			m_frameStats.m_prepareBucketsEndTime = GetSystem<TimeSystem>()->GetElapsedTimeReal();
		}
	}

	void MeshRenderer::PrepareForRendering(class RenderPassContext& ctx)
	{
		R3_PROF_EVENT();
		if (m_drawIndirectHostVisible.m_mappedBuffer == nullptr)
		{
			auto drawBuffer = GetSystem<RenderSystem>()->GetBufferPool()->GetBuffer("Mesh draw indirect",
				c_maxInstances * c_maxBuffers * sizeof(VkDrawIndexedIndirectCommand),
				VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
				VMA_MEMORY_USAGE_AUTO, true);
			if (!drawBuffer)
			{
				LogError("Failed to allocate draw indirect buffer");
				return;
			}
			m_drawIndirectHostVisible = *drawBuffer;
			m_drawIndirectBufferAddress = m_drawIndirectHostVisible.m_deviceAddress;
		}
		if (m_pipelineLayout == VK_NULL_HANDLE || m_pipelineLayoutWithShadowmaps == VK_NULL_HANDLE)
		{
			if (!CreatePipelineLayouts(*ctx.m_device))
			{
				LogError("Failed to create mesh pipeline layout");
				return;
			}
		}
		if (!m_staticMeshInstances.IsCreated())
		{
			if (!m_staticMeshInstances.Create("Static mesh instances", *ctx.m_device, c_maxInstances, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT))
			{
				LogError("Failed to create static mesh instance buffer");
				return;
			}
		}
		if (!m_staticMaterialOverrides.IsCreated())
		{
			if (!m_staticMaterialOverrides.Create("Static material overrides", *ctx.m_device, c_maxStaticMaterialOverrides, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT))
			{
				LogError("Failed to create static mesh instance buffer");
				return;
			}
		}
		if (!m_dynamicMeshInstances.IsCreated())
		{
			if (!m_dynamicMeshInstances.Create("Dynamic mesh instances", *ctx.m_device, c_maxInstances, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT))
			{
				LogError("Failed to create dynamic mesh instance buffer");
				return;
			}
		}
		if (!m_globalsBuffer.IsCreated())
		{
			if (!m_globalsBuffer.Create("Mesh render globals", *ctx.m_device, c_maxGlobalsPerFrame, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT))
			{
				LogError("Failed to create mesh reder globals bufer");
				return;
			}
		}

		// old static data buffers have been retired, flush writes to the new ones
		if (m_rebuildingStaticScene)
		{
			m_staticMaterialOverrides.Flush(*ctx.m_device, ctx.m_graphicsCmds, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
			if ((m_staticOpaques.m_partInstances.size() > 0 || m_staticTransparents.m_partInstances.size() > 0 || m_staticShadowCasters.m_partInstances.size() > 0))
			{
				m_staticMeshInstances.Flush(*ctx.m_device, ctx.m_graphicsCmds, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
			}
			m_rebuildingStaticScene = false;
		}

		// Flush dynamic data every frame
		if ((m_dynamicOpaques.m_partInstances.size() > 0 || m_dynamicTransparents.m_partInstances.size() > 0 || m_dynamicShadowCasters.m_partInstances.size() > 0))
		{
			m_dynamicMeshInstances.Flush(*ctx.m_device, ctx.m_graphicsCmds, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
		}

		// Sadly we cannot globals during rendering, so we do them all now. Different passes need different globals data
		auto cameras = GetSystem<CameraSystem>();
		auto staticMeshes = GetSystem<StaticMeshSystem>();
		auto lights = GetSystem<LightsSystem>();

		ShaderGlobals globals;
		// main camera + lights globals
		globals.m_worldToViewTransform = cameras->GetMainCamera().ViewMatrix();
		globals.m_projViewTransform = cameras->GetMainCamera().ProjectionMatrix() * cameras->GetMainCamera().ViewMatrix();
		globals.m_cameraWorldSpacePos = glm::vec4(cameras->GetMainCamera().Position(), 1);
		globals.m_vertexPosUVBufferAddress = staticMeshes->GetVertexPosUVDeviceAddress();
		globals.m_verteNormTangentBufferAddress = staticMeshes->GetVertexNormTangentDeviceAddress();
		globals.m_lightDataBufferAddress = lights->GetAllLightsDeviceAddress();
		globals.m_lightTileMetadataAddress = m_lightTileMetadata;
		m_globalsBuffer.Write(1, &globals);
		m_thisFrameMainCameraGlobals = m_globalsBuffer.GetBufferDeviceAddress();

		// shadow cascade globals
		globals.m_cameraWorldSpacePos = { 0,0,0,0 };	// unused in shadow passes
		globals.m_worldToViewTransform = {};			// ^^ 
		globals.m_lightDataBufferAddress = 0;			// ^^
		globals.m_lightTileMetadataAddress = 0;			// ^^
		assert(lights->GetShadowCascadeCount() <= 4);
		for (int cascade = 0; cascade < lights->GetShadowCascadeCount(); ++cascade)
		{
			globals.m_projViewTransform = lights->GetShadowCascadeMatrix(cascade);
			m_globalsBuffer.Write(1, &globals);
			m_shadowCascadeGlobals[cascade] = m_globalsBuffer.GetBufferDeviceAddress() + (sizeof(ShaderGlobals) * (cascade + 1));
		}
		
		// flush all writes in one go
		m_globalsBuffer.Flush(*ctx.m_device, ctx.m_graphicsCmds, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
		m_globalsBuffer.RetirePooledBuffer(*ctx.m_device);	// aquire new buffer after write
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
		pc.m_globalsBuffer = m_thisFrameMainCameraGlobals;
		pc.m_instanceDataBufferAddress = m_staticMeshInstances.GetBufferDeviceAddress();

		vkCmdPushConstants(ctx.m_graphicsCmds, m_pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pc), &pc);
		vkCmdDrawIndexedIndirect(ctx.m_graphicsCmds, m_drawIndirectHostVisible.m_buffer.m_buffer, m_staticOpaqueDrawData.m_firstDrawOffset * sizeof(VkDrawIndexedIndirectCommand), m_staticOpaqueDrawData.m_drawCount, sizeof(VkDrawIndexedIndirectCommand));

		pc.m_instanceDataBufferAddress = m_dynamicMeshInstances.GetBufferDeviceAddress();
		vkCmdPushConstants(ctx.m_graphicsCmds, m_pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pc), &pc);
		vkCmdDrawIndexedIndirect(ctx.m_graphicsCmds, m_drawIndirectHostVisible.m_buffer.m_buffer, m_dynamicOpaqueDrawData.m_firstDrawOffset * sizeof(VkDrawIndexedIndirectCommand), m_dynamicOpaqueDrawData.m_drawCount, sizeof(VkDrawIndexedIndirectCommand));

		m_frameStats.m_writeGBufferCmdsEndTime = time->GetElapsedTimeReal();
	}

	void MeshRenderer::OnShadowCascadeDraw(RenderPassContext& ctx, const RenderTargetInfo& target, int cascade)
	{
		R3_PROF_EVENT();

		auto lights = GetSystem<LightsSystem>();
		glm::mat4 cascadeMatrix = GetSystem<LightsSystem>()->GetShadowCascadeMatrix(cascade);
		float biasConstant = 0.0f, biasClamp = 0.0f, biasSlope = 0.0f;
		lights->GetShadowCascadeDepthBiasSettings(cascade, biasConstant, biasClamp, biasSlope);
		OnShadowMapDraw(ctx, target, cascadeMatrix, biasConstant, biasClamp, biasSlope, m_staticSunShadowCastersDrawData[cascade], m_dynamicSunShadowCastersDrawData[cascade], m_shadowCascadeGlobals[cascade]);
	}

	void MeshRenderer::OnShadowMapDraw(class RenderPassContext& ctx, const RenderTargetInfo& target, glm::mat4 shadowMatrix, float depthBiasConstant, float depthBiasClamp, float depthBiasSlope, const MeshPartBucketDrawIndirects& staticDraws, const MeshPartBucketDrawIndirects& dynamicDraws, VkDeviceAddress globals)
	{
		R3_PROF_EVENT();

		auto actualTarget = ctx.GetResolvedTarget(target);
		if (actualTarget == nullptr)
		{
			LogError("Failed to get resolved target");
			return;
		}

		if (m_shadowPipeline == VK_NULL_HANDLE && !CreateShadowPipelineData(*ctx.m_device, actualTarget->m_info.m_format))
		{
			LogError("Failed to create pipeline data for shadow pass");
			return;
		}

		if (staticDraws.m_drawCount == 0 && dynamicDraws.m_drawCount == 0)
		{
			return;
		}

		auto textures = GetSystem<TextureSystem>();
		auto time = GetSystem<TimeSystem>();
		auto staticMeshes = GetSystem<StaticMeshSystem>();

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

		vkCmdBindPipeline(ctx.m_graphicsCmds, VK_PIPELINE_BIND_POINT_GRAPHICS, m_shadowPipeline);
		vkCmdSetViewport(ctx.m_graphicsCmds, 0, 1, &viewport);
		vkCmdSetScissor(ctx.m_graphicsCmds, 0, 1, &scissor);
		vkCmdSetDepthBias(ctx.m_graphicsCmds, depthBiasConstant, depthBiasClamp, depthBiasSlope);
		
		vkCmdBindIndexBuffer(ctx.m_graphicsCmds, GetSystem<StaticMeshSystem>()->GetIndexBuffer(), 0, VK_INDEX_TYPE_UINT32);
		VkDescriptorSet allTextures = textures->GetAllTexturesSet();
		vkCmdBindDescriptorSets(ctx.m_graphicsCmds, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &allTextures, 0, nullptr);

		PushConstants pc;
		pc.m_globalsBuffer = globals;
		pc.m_instanceDataBufferAddress = m_staticMeshInstances.GetBufferDeviceAddress();

		vkCmdPushConstants(ctx.m_graphicsCmds, m_pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pc), &pc);
		vkCmdDrawIndexedIndirect(ctx.m_graphicsCmds, m_drawIndirectHostVisible.m_buffer.m_buffer, staticDraws.m_firstDrawOffset * sizeof(VkDrawIndexedIndirectCommand), staticDraws.m_drawCount, sizeof(VkDrawIndexedIndirectCommand));

		pc.m_instanceDataBufferAddress = m_dynamicMeshInstances.GetBufferDeviceAddress();
		vkCmdPushConstants(ctx.m_graphicsCmds, m_pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pc), &pc);
		vkCmdDrawIndexedIndirect(ctx.m_graphicsCmds, m_drawIndirectHostVisible.m_buffer.m_buffer, dynamicDraws.m_firstDrawOffset * sizeof(VkDrawIndexedIndirectCommand), dynamicDraws.m_drawCount, sizeof(VkDrawIndexedIndirectCommand));

		m_frameStats.m_writeForwardCmdsEndTime = time->GetElapsedTimeReal();
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
		VkDescriptorSet allShadowMaps = lights->GetAllShadowMapsSet();
		vkCmdBindDescriptorSets(ctx.m_graphicsCmds, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayoutWithShadowmaps, 0, 1, &allTextures, 0, nullptr);
		vkCmdBindDescriptorSets(ctx.m_graphicsCmds, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayoutWithShadowmaps,1, 1, &allShadowMaps, 0, nullptr);

		PushConstants pc;
		pc.m_globalsBuffer = m_thisFrameMainCameraGlobals;
		pc.m_instanceDataBufferAddress = m_staticMeshInstances.GetBufferDeviceAddress();

		vkCmdPushConstants(ctx.m_graphicsCmds, m_pipelineLayoutWithShadowmaps, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pc), &pc);
		vkCmdDrawIndexedIndirect(ctx.m_graphicsCmds, m_drawIndirectHostVisible.m_buffer.m_buffer, m_staticTransparentDrawData.m_firstDrawOffset * sizeof(VkDrawIndexedIndirectCommand), m_staticTransparentDrawData.m_drawCount, sizeof(VkDrawIndexedIndirectCommand));
		
		pc.m_instanceDataBufferAddress = m_dynamicMeshInstances.GetBufferDeviceAddress();
		vkCmdPushConstants(ctx.m_graphicsCmds, m_pipelineLayoutWithShadowmaps, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pc), &pc);
		vkCmdDrawIndexedIndirect(ctx.m_graphicsCmds, m_drawIndirectHostVisible.m_buffer.m_buffer, m_dynamicTransparentDrawData.m_firstDrawOffset * sizeof(VkDrawIndexedIndirectCommand), m_dynamicTransparentDrawData.m_drawCount, sizeof(VkDrawIndexedIndirectCommand));

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

						if (meshMaterial->m_flags & (uint32_t)MeshMaterialFlags::CastShadows)
						{
							if constexpr (std::is_same<MeshCmpType, StaticMeshComponent>::value)
							{
								m_staticShadowCasters.m_partInstances.emplace_back(bucketInstance);
							}
							else
							{
								m_dynamicShadowCasters.m_partInstances.emplace_back(bucketInstance);
							}
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
		m_staticTransparents.m_partInstances.clear();
		m_staticShadowCasters.m_partInstances.clear();
		RebuildStaticMaterialOverrides();
		RebuildInstances<StaticMeshComponent, false>(m_staticMeshInstances, m_staticOpaques, m_staticTransparents);
	}

	// must be called after RebuildStaticScene to get proper material updates after scene rebuild
	void MeshRenderer::RebuildDynamicScene()
	{
		m_dynamicOpaques.m_partInstances.clear();
		m_dynamicTransparents.m_partInstances.clear();
		m_dynamicShadowCasters.m_partInstances.clear();
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
			VkDrawIndexedIndirectCommand* drawPtr = static_cast<VkDrawIndexedIndirectCommand*>(m_drawIndirectHostVisible.m_mappedBuffer) + currentDrawBufferStart + m_currentDrawBufferOffset;
			drawPtr->indexCount = currentPartData->m_indexCount;
			drawPtr->instanceCount = 1;
			drawPtr->firstIndex = (uint32_t)currentPartData->m_indexStartOffset;
			drawPtr->vertexOffset = currentPartData->m_vertexDataOffset;
			drawPtr->firstInstance = bucketInstance.m_partInstanceIndex;
			m_currentDrawBufferOffset++;
		}
	}

	// use compute to cull and prepare draw calls for instances in this bucket
	void MeshRenderer::PrepareAndCullDrawBucketCompute(Device& d, VkCommandBuffer cmds, const Frustum& f, VkDeviceAddress instanceDataBuffer, const MeshPartInstanceBucket& bucket, MeshPartBucketDrawIndirects& drawData)
	{
		R3_PROF_EVENT();

		const uint32_t currentDrawBufferStart = m_thisFrameBuffer * c_maxInstances;
		drawData.m_firstDrawOffset = (currentDrawBufferStart + m_currentDrawBufferOffset);	// record draw-indirect base offset for this bucket
		drawData.m_drawCount = (uint32_t)bucket.m_partInstances.size();						// draw-indirects will be populated by compute shader
		m_currentDrawBufferOffset += drawData.m_drawCount;

		m_computeCulling->Run(d, cmds, instanceDataBuffer, m_drawIndirectBufferAddress, bucket, drawData, f);	// populate the draw-indirect data for this bucket of instances via compute
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
		m_frameStats.m_totalStaticInstances = (uint32_t)(m_staticOpaques.m_partInstances.size() + m_staticTransparents.m_partInstances.size());
		m_frameStats.m_totalDynamicInstances = (uint32_t)(m_dynamicOpaques.m_partInstances.size() + m_dynamicTransparents.m_partInstances.size());
		m_frameStats.m_totalStaticShadowCasters = (uint32_t)(m_staticShadowCasters.m_partInstances.size());
		m_frameStats.m_totalDynamicShadowCasters = (uint32_t)(m_dynamicShadowCasters.m_partInstances.size());
		m_frameStats.m_totalPartInstances = m_frameStats.m_totalOpaqueInstances + m_frameStats.m_totalTransparentInstances;
		m_frameStats.m_collectInstancesEndTime = GetSystem<TimeSystem>()->GetElapsedTimeReal();
		m_frameStats.m_prepareBucketsStartTime = GetSystem<TimeSystem>()->GetElapsedTimeReal();
		if (!m_enableComputeCulling)		// do nothing here, gpu culling happens later
		{
			PrepareDrawBucket(m_staticOpaques, m_staticOpaqueDrawData);
			PrepareDrawBucket(m_staticTransparents, m_staticTransparentDrawData);
			PrepareDrawBucket(m_dynamicOpaques, m_dynamicOpaqueDrawData);
			PrepareDrawBucket(m_dynamicTransparents, m_dynamicTransparentDrawData);
		}
		if(!m_enableLightCascadeCulling)
		{
			auto lights = GetSystem<LightsSystem>();
			const int cascades = lights->GetShadowCascadeCount();
			for (int i = 0; i < cascades; ++i)
			{
				PrepareDrawBucket(m_staticShadowCasters, m_staticSunShadowCastersDrawData[i]);
				PrepareDrawBucket(m_dynamicShadowCasters, m_dynamicSunShadowCastersDrawData[i]);
			}
		}
		m_frameStats.m_prepareBucketsEndTime = GetSystem<TimeSystem>()->GetElapsedTimeReal();

		return true;
	}
}