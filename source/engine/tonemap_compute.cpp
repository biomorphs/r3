#include "tonemap_compute.h"
#include "render/device.h"
#include "render/render_target_cache.h"
#include "render/vulkan_helpers.h"
#include "core/profiler.h"
#include "core/log.h"

namespace R3
{
	bool TonemapCompute::Initialise(Device& d)
	{
		R3_PROF_EVENT();

		m_descriptorAllocator = std::make_unique<DescriptorSetSimpleAllocator>();
		std::vector<VkDescriptorPoolSize> poolSizes = {							
			{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 2 * c_maxSets }					// input + output images
		};
		if (!m_descriptorAllocator->Initialise(d, c_maxSets, poolSizes))
		{
			LogError("Failed to create descriptor allocator");
			return false;
		}

		DescriptorLayoutBuilder layoutBuilder;
		layoutBuilder.AddBinding(0, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);		// input image
		layoutBuilder.AddBinding(1, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);		// output image
		m_descriptorLayout = layoutBuilder.Create(d, false);					// dont need bindless here
		if (m_descriptorLayout == nullptr)
		{
			LogError("Failed to create descriptor set layout");
			return false;
		}

		// Create the sets but don't write them yet
		for (uint32_t i = 0; i < c_maxSets; ++i)
		{
			m_descriptorSets[i] = m_descriptorAllocator->Allocate(d, m_descriptorLayout);
		}

		auto computeShader = VulkanHelpers::LoadShaderModule(d.GetVkDevice(), "shaders_spirv/common/tonemap_compute.comp.spv");
		if (computeShader == VK_NULL_HANDLE)
		{
			LogError("Failed to load tonemap shader");
			return false;
		}

		// Create the pipeline
		VkPipelineLayoutCreateInfo pipelineLayoutInfo = { 0 };
		pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutInfo.pSetLayouts = &m_descriptorLayout;
		pipelineLayoutInfo.setLayoutCount = 1;
		if (!VulkanHelpers::CheckResult(vkCreatePipelineLayout(d.GetVkDevice(), &pipelineLayoutInfo, nullptr, &m_pipelineLayout)))
		{
			LogError("Failed to create pipeline layout");
			return false;
		}
		m_pipeline = VulkanHelpers::CreateComputePipeline(d.GetVkDevice(), computeShader, m_pipelineLayout, "main");

		// We don't need the shader any more
		vkDestroyShaderModule(d.GetVkDevice(), computeShader, nullptr);

		m_resourcesInitialised = true;
		return true;
	}

	void TonemapCompute::Run(Device& d, VkCommandBuffer cmds, RenderTarget& hdrTarget, glm::vec2 hdrDimensions, RenderTarget& outputTarget, glm::vec2 outputDimensions)
	{
		R3_PROF_EVENT();
		
		if (!m_resourcesInitialised)
		{
			if (!Initialise(d))
			{
				LogError("Failed to initialise tonemap compute shader!");
				return;
			}
		}

		// Write a descriptor set each frame
		DescriptorSetWriter writer(m_descriptorSets[m_currentSet]);
		writer.WriteStorageImage(0, hdrTarget.m_view, hdrTarget.m_lastLayout);
		writer.WriteStorageImage(1, outputTarget.m_view, outputTarget.m_lastLayout);
		writer.FlushWrites();

		auto dimensions = glm::min(hdrDimensions,outputDimensions);
		vkCmdBindPipeline(cmds, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipeline);
		vkCmdBindDescriptorSets(cmds, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipelineLayout, 0, 1, &m_descriptorSets[m_currentSet], 0, nullptr);
		vkCmdDispatch(cmds, (uint32_t)glm::ceil(dimensions.x / 16.0f), (uint32_t)glm::ceil(dimensions.y / 16.0f), 1);
		if (++m_currentSet >= c_maxSets)
		{
			m_currentSet = 0;
		}
	}

	void TonemapCompute::Cleanup(Device& d)
	{
		R3_PROF_EVENT();

		// cleanup the pipeline
		vkDestroyPipeline(d.GetVkDevice(), m_pipeline, nullptr);
		vkDestroyPipelineLayout(d.GetVkDevice(), m_pipelineLayout, nullptr);

		// cleanup the descriptors
		vkDestroyDescriptorSetLayout(d.GetVkDevice(), m_descriptorLayout, nullptr);
		m_descriptorAllocator = {};
	}
}