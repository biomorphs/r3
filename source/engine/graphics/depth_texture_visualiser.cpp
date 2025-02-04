#include "depth_texture_visualiser.h"
#include "render/vulkan_helpers.h"
#include "render/render_target_cache.h"
#include "render/device.h"
#include "core/profiler.h"
#include "core/log.h"
#include <imgui.h>

namespace R3
{
	struct PushConstants
	{
		float m_minValue;
		float m_maxValue;
		float m_scale;
		glm::vec2 m_offset;
	};

	void DepthTextureVisualiser::ShowGui()
	{
		R3_PROF_EVENT();
		ImGui::DragFloat("Min Depth", &m_minValue, 0.001f, 0.0f, m_maxValue);
		ImGui::DragFloat("Max Depth", &m_maxValue, 0.001f, m_minValue, 1.0f);
		ImGui::DragFloat("Scale", &m_scale, 0.01f, 0.01f, 4.0f);
		ImGui::DragFloat2("Offset", glm::value_ptr(m_offset), 0.01f, -1.0f, 1.0f);
	}

	void DepthTextureVisualiser::Run(Device& d, VkCommandBuffer cmds, RenderTarget& depthBuffer, glm::vec2 depthDimensions, RenderTarget& outputTarget, glm::vec2 outputDimensions)
	{
		R3_PROF_EVENT();
		if (!m_resourcesInitialised && !Initialise(d))
		{
			LogError("Failed to initialise depth texture visualiser!");
			return;
		}

		// Write a descriptor set each frame
		DescriptorSetWriter writer(m_descriptorSets[m_currentSet]);
		writer.WriteImage(0, 0, depthBuffer.m_view, m_depthSampler, depthBuffer.m_lastLayout);
		writer.WriteStorageImage(1, outputTarget.m_view, outputTarget.m_lastLayout);
		writer.FlushWrites();

		PushConstants pc;
		pc.m_minValue = m_minValue;
		pc.m_maxValue = m_maxValue;
		pc.m_offset = m_offset;
		pc.m_scale = m_scale;

		vkCmdBindPipeline(cmds, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipeline);
		vkCmdBindDescriptorSets(cmds, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipelineLayout, 0, 1, &m_descriptorSets[m_currentSet], 0, nullptr);
		vkCmdPushConstants(cmds, m_pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pc), &pc);
		vkCmdDispatch(cmds, (uint32_t)glm::ceil(outputDimensions.x / 16.0f), (uint32_t)glm::ceil(outputDimensions.y / 16.0f), 1);
		if (++m_currentSet >= c_maxSets)
		{
			m_currentSet = 0;
		}
	}

	void DepthTextureVisualiser::Cleanup(Device& d)
	{
		R3_PROF_EVENT();

		vkDestroySampler(d.GetVkDevice(), m_depthSampler, nullptr);
		vkDestroyPipeline(d.GetVkDevice(), m_pipeline, nullptr);
		vkDestroyPipelineLayout(d.GetVkDevice(), m_pipelineLayout, nullptr);
		vkDestroyDescriptorSetLayout(d.GetVkDevice(), m_descriptorLayout, nullptr);
		m_descriptorAllocator = {};
	}

	bool DepthTextureVisualiser::Initialise(Device& d)
	{
		R3_PROF_EVENT();

		m_descriptorAllocator = std::make_unique<DescriptorSetSimpleAllocator>();
		std::vector<VkDescriptorPoolSize> poolSizes = {
			{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, c_maxSets },					// output images
			{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, c_maxSets }			// depth buffer
		};
		if (!m_descriptorAllocator->Initialise(d, c_maxSets, poolSizes))
		{
			LogError("Failed to create descriptor allocator");
			return false;
		}

		DescriptorLayoutBuilder layoutBuilder;
		layoutBuilder.AddBinding(0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);		// depth buffer texture
		layoutBuilder.AddBinding(1, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);				// output image
		m_descriptorLayout = layoutBuilder.Create(d, false);
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

		// create depth sampler
		{
			VkSamplerCreateInfo sampler = {};
			sampler.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
			sampler.magFilter = VK_FILTER_NEAREST;
			sampler.minFilter = VK_FILTER_NEAREST;
			sampler.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
			sampler.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
			sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
			sampler.maxLod = VK_LOD_CLAMP_NONE;
			sampler.minLod = 0;
			if (!VulkanHelpers::CheckResult(vkCreateSampler(d.GetVkDevice(), &sampler, nullptr, &m_depthSampler)))
			{
				LogError("Failed to create sampler");
				return false;
			}
		}

		auto computeShader = VulkanHelpers::LoadShaderModule(d.GetVkDevice(), "shaders_spirv/common/visualise_depth_texture.comp.spv");
		if (computeShader == VK_NULL_HANDLE)
		{
			LogError("Failed to load depth texture visualiser shader");
			return false;
		}

		// Create the pipelines and layouts
		VkPushConstantRange constantRange;
		constantRange.offset = 0;
		constantRange.size = sizeof(PushConstants);
		constantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

		VkPipelineLayoutCreateInfo pipelineLayoutInfo = { 0 };
		pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutInfo.pSetLayouts = &m_descriptorLayout;
		pipelineLayoutInfo.setLayoutCount = 1;
		pipelineLayoutInfo.pushConstantRangeCount = 1;
		pipelineLayoutInfo.pPushConstantRanges = &constantRange;
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
}