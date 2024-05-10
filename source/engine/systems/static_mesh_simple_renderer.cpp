#include "static_mesh_simple_renderer.h"
#include "static_mesh_system.h"
#include "camera_system.h"
#include "lights_system.h"
#include "engine/components/transform.h"
#include "engine/components/static_mesh.h"
#include "engine/components/static_mesh_materials.h"
#include "entities/systems/entity_system.h"
#include "entities/world.h"
#include "entities/queries.h"
#include "render/render_system.h"
#include "render/device.h"
#include "render/pipeline_builder.h"
#include "core/profiler.h"
#include "core/log.h"
#include <imgui.h>

namespace R3
{
	// todo 
	// add materials data WriteOnlyGpuArray
	//	make sure per-instance material-override is possible
	//	 allocate material entry per-instance?
	// add lights data WriteOnlyGpuArray
	//	or most likely, a lights system

	// stored in a buffer
	struct StaticMeshSimpleRenderer::GlobalConstants
	{
		glm::mat4 m_projViewTransform;
		glm::vec4 m_cameraWorldSpacePos;
		VkDeviceAddress m_vertexBufferAddress;
		VkDeviceAddress m_materialBufferAddress;
		VkDeviceAddress m_pointLightsBufferAddress;
		uint32_t m_firstPointLightOffset;
		uint32_t m_pointlightCount;
	};

	// one per draw call
	struct PushConstants 
	{
		glm::mat4 m_instanceTransform;
		VkDeviceAddress m_globalConstantsAddress;
		uint32_t m_globalIndex;
		uint32_t m_materialIndex;
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
	}

	bool StaticMeshSimpleRenderer::ShowGui()
	{
		R3_PROF_EVENT();
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
		// Create pipeline layout
		VkPushConstantRange constantRange;
		constantRange.offset = 0;	// needs to match in the shader if >0!
		constantRange.size = sizeof(PushConstants);
		constantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
		VkPipelineLayoutCreateInfo pipelineLayoutInfo = { 0 };
		pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutInfo.pushConstantRangeCount = 1;
		pipelineLayoutInfo.pPushConstantRanges = &constantRange;
		if (!VulkanHelpers::CheckResult(vkCreatePipelineLayout(d.GetVkDevice(), &pipelineLayoutInfo, nullptr, &m_pipelineLayout)))
		{
			LogInfo("Failed to create pipeline layout");
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

	void StaticMeshSimpleRenderer::MainPassBegin(Device& d, VkCommandBuffer cmds)
	{
		R3_PROF_EVENT();
		if (m_pipelineLayout == VK_NULL_HANDLE || m_simpleTriPipeline == VK_NULL_HANDLE)
		{
			CreatePipelineData(d);
		}
		if (!m_globalConstantsBuffer.IsCreated())
		{
			if (!m_globalConstantsBuffer.Create(d, c_maxGlobalConstantBuffers, c_maxGlobalConstantBuffers * 2, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT))
			{
				LogError("Failed to create constant buffer");
			}
			else
			{
				m_globalConstantsBuffer.Allocate(c_maxGlobalConstantBuffers);	// reserve the memory now, allows writes to any entry
			}
		}
		else
		{
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
			m_globalConstantsBuffer.Write(m_currentGlobalConstantsBuffer, 1, &globals);
			m_globalConstantsBuffer.Flush(d, cmds);
		}
	}

	void StaticMeshSimpleRenderer::MainPassDraw(Device& d, VkCommandBuffer cmds, const VkExtent2D& e)
	{
		R3_PROF_EVENT();
		if (m_simpleTriPipeline == VK_NULL_HANDLE)
		{
			return;
		}
		auto cameras = GetSystem<CameraSystem>();
		auto staticMeshes = GetSystem<StaticMeshSystem>();
		auto entities = Systems::GetSystem<Entities::EntitySystem>();
		auto activeWorld = entities->GetActiveWorld();
		const auto viewProjMat = cameras->GetMainCamera().ProjectionMatrix() * cameras->GetMainCamera().ViewMatrix();

		VkViewport viewport = { 0 };
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = (float)e.width;
		viewport.height = (float)e.height;
		viewport.minDepth = 0.0f;	// normalised! must be between 0 and 1
		viewport.maxDepth = 1.0f;	// ^^
		VkRect2D scissor = { 0 };
		scissor.offset = { 0, 0 };
		scissor.extent = e;	// draw the full image

		vkCmdBindPipeline(cmds, VK_PIPELINE_BIND_POINT_GRAPHICS, m_simpleTriPipeline);
		vkCmdSetViewport(cmds, 0, 1, &viewport);
		vkCmdSetScissor(cmds, 0, 1, &scissor);
		vkCmdBindIndexBuffer(cmds, staticMeshes->GetIndexBuffer(), 0, VK_INDEX_TYPE_UINT32);
		
		// pass the vertex buffer and model-view-proj matrix via push constants for each instance
		PushConstants pc;
		pc.m_globalConstantsAddress = m_globalConstantsBuffer.GetDataDeviceAddress();
		pc.m_globalIndex = m_currentGlobalConstantsBuffer++;
		if (m_currentGlobalConstantsBuffer >= c_maxGlobalConstantBuffers)
		{
			m_currentGlobalConstantsBuffer = 0;
		}
		if (activeWorld)
		{
			StaticMeshGpuData currentMeshData;
			ModelDataHandle currentMeshDataHandle;
			StaticMeshPart partData;
			auto forEach = [&](const Entities::EntityHandle& e, StaticMeshComponent& s, TransformComponent& t) 
			{
				if (s.m_modelHandle.m_index != -1 && s.m_modelHandle.m_index != currentMeshDataHandle.m_index)
				{
					if (staticMeshes->GetMeshDataForModel(s.m_modelHandle, currentMeshData))
					{
						currentMeshDataHandle = s.m_modelHandle;
					}
					else
					{
						currentMeshData = {};
					}
				}
				if (s.m_modelHandle.m_index != -1 && currentMeshDataHandle.m_index == s.m_modelHandle.m_index)
				{
					const uint32_t partCount = currentMeshData.m_meshPartCount;
					const auto* matCmp = activeWorld->GetComponent<StaticMeshMaterialsComponent>(s.m_materialOverride);
					bool useOverrides = matCmp && matCmp->m_gpuDataIndex != -1 && matCmp->m_materials.size() >= partCount;
					for (uint32_t part = 0; part < partCount; ++part)
					{
						if (staticMeshes->GetMeshPart(currentMeshData.m_firstMeshPartOffset + part, partData))
						{
							pc.m_materialIndex = useOverrides ? ((uint32_t)matCmp->m_gpuDataIndex + part) : partData.m_materialIndex;
							pc.m_instanceTransform = t.GetWorldspaceInterpolated() * partData.m_transform;
							vkCmdPushConstants(cmds, m_pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pc), &pc);
							vkCmdDrawIndexed(cmds, partData.m_indexCount, 1, (uint32_t)partData.m_indexStartOffset, (uint32_t)currentMeshData.m_vertexDataOffset, 0);
						}
					}
				}

				return true;
			};
			Entities::Queries::ForEach<StaticMeshComponent, TransformComponent>(activeWorld, forEach);
		}
	}
}