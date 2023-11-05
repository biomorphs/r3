#pragma once 
#include <vulkan/vulkan.h>
#include <vector>

namespace R3
{
	// simplifies creation of pipeline objects
	class PipelineBuilder
	{
	public:
		std::vector<VkPipelineShaderStageCreateInfo> m_shaderStages;
		VkPipelineDynamicStateCreateInfo m_dynamicState = { 0 };
		VkPipelineVertexInputStateCreateInfo m_vertexInputState = { 0 };
		VkPipelineInputAssemblyStateCreateInfo m_inputAssemblyState = { 0 };
		VkPipelineViewportStateCreateInfo m_viewportState = { 0 };
		VkPipelineRasterizationStateCreateInfo m_rasterState = { 0 };
		VkPipelineMultisampleStateCreateInfo m_multisamplingState = { 0 };
		VkPipelineDepthStencilStateCreateInfo m_depthStencilState = { 0 };
		VkPipelineColorBlendStateCreateInfo m_colourBlendState = { 0 };
		
		VkPipeline Build(VkDevice device, VkPipelineLayout layout, VkRenderPass pass, int subPass = 0);
		VkPipeline Build(VkDevice device, VkPipelineLayout layout, int colourAttachCount, VkFormat* colourAttachFormats, VkFormat depthAttachFormat);
	};
}