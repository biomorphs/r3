#pragma once

// yeah, get over it, if you want these you already have this included 99% of the time
#include <vulkan/vulkan.h>
#include <vector>
#include <string_view>

namespace R3
{
	// our aim is NOT to abstract away vulkan, just hide the annoying bits
	namespace VulkanHelpers
	{
		// ShaderModule wraps the spirv and can be deleted once a pipeline is built with it
		VkShaderModule CreateShaderModule(VkDevice device, const std::vector<uint8_t>& srcSpirv);
		VkShaderModule LoadShaderModule(VkDevice device, std::string_view filePath);
		
		// Pipeline stuff
		VkPipelineShaderStageCreateInfo CreatePipelineShaderState(VkShaderStageFlagBits stage, VkShaderModule shader, const char* entryPoint = "main");
		VkPipelineDynamicStateCreateInfo CreatePipelineDynamicState(const std::vector<VkDynamicState>& statesToEnable);
		VkPipelineInputAssemblyStateCreateInfo CreatePipelineInputAssemblyState(VkPrimitiveTopology topology, bool enablePrimitiveRestart=false);
		VkPipelineMultisampleStateCreateInfo CreatePipelineMultiSampleState_SingleSample();
		VkPipelineRasterizationStateCreateInfo CreatePipelineRasterState(VkPolygonMode polyMode, VkCullModeFlags cullMode, VkFrontFace frontFace);
		VkPipelineColorBlendAttachmentState CreatePipelineColourBlendAttachment_NoBlending();
		VkPipelineColorBlendStateCreateInfo CreatePipelineColourBlendState(const std::vector<VkPipelineColorBlendAttachmentState>& attachments);
	}
}