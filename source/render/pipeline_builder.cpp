#include "pipeline_builder.h"
#include "core/profiler.h"
#include <vulkan/vk_enum_string_helper.h>
#include <fmt/format.h>

namespace R3
{
	VkPipeline PipelineBuilder::Build(VkDevice device, VkPipelineLayout layout, VkRenderPass pass, int subPass)
	{
		R3_PROF_EVENT();
		VkPipeline pipeline = VK_NULL_HANDLE;
		VkGraphicsPipelineCreateInfo pipelineInfo{};
		pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		pipelineInfo.stageCount = m_shaderStages.size();
		pipelineInfo.pStages = m_shaderStages.data();
		pipelineInfo.pVertexInputState = &m_vertexInputState;
		pipelineInfo.pInputAssemblyState = &m_inputAssemblyState;
		pipelineInfo.pViewportState = &m_viewportState;
		pipelineInfo.pRasterizationState = &m_rasterState;
		pipelineInfo.pMultisampleState = &m_multisamplingState;
		pipelineInfo.pDepthStencilState = &m_depthStencilState;
		pipelineInfo.pColorBlendState = &m_colourBlendState;
		pipelineInfo.pDynamicState = &m_dynamicState;
		pipelineInfo.layout = layout;
		pipelineInfo.renderPass = pass;		// pipeline is tied to a specific render pass/subpass!
		pipelineInfo.subpass = subPass;
		pipelineInfo.basePipelineHandle = VK_NULL_HANDLE; // used for pipeline derivatives, we dont care for now
		pipelineInfo.basePipelineIndex = -1; // ^^
		VkResult r = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline);
		if(r) 
		{
			std::string error = string_VkResult(r);
			fmt::print("failed to create graphics pipeline! {}\n", error);
			return VK_NULL_HANDLE;
		}
		return pipeline;
	}
}