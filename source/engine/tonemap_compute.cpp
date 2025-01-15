#include "tonemap_compute.h"
#include "render/device.h"
#include "render/render_target_cache.h"
#include "render/vulkan_helpers.h"
#include "core/profiler.h"
#include "core/log.h"
#include <imgui.h>

namespace R3
{
	static const std::string_view c_toneMapTypeNames[] = {
			"Reinhard (Colour)",
			"Reinhard (Luminance)",
			"AGX",
			"AGX Golden",
			"AGX Punchy",
			"Uncharted 2 Filmic",
			"ACES Approx",
			"ACES Fitted"
	};

	static const std::string_view c_toneMapShaders[] = {
		"shaders_spirv/common/tonemap_reinhard_compute.comp.spv",
		"shaders_spirv/common/tonemap_reinhard_luminance_compute.comp.spv",
		"shaders_spirv/common/tonemap_agx_compute.comp.spv",
		"shaders_spirv/common/tonemap_agx_golden_look_compute.comp.spv",
		"shaders_spirv/common/tonemap_agx_punchy_look_compute.comp.spv",
		"shaders_spirv/common/tonemap_uncharted_compute.comp.spv",
		"shaders_spirv/common/tonemap_aces_approx_compute.comp.spv",
		"shaders_spirv/common/tonemap_aces_fitted_compute.comp.spv"
	};

	void TonemapCompute::ShowGui()
	{
		static_assert(std::size(c_toneMapTypeNames) == MaxTonemapTypes);
		assert(m_type < std::size(c_toneMapTypeNames));
		ImGui::Begin("Tonemapper");
		if (ImGui::BeginCombo("Type", c_toneMapTypeNames[m_type].data()))
		{
			for (int type = 0; type < std::size(c_toneMapTypeNames); ++type)
			{
				bool selected = (type == m_type);
				if (ImGui::Selectable(c_toneMapTypeNames[type].data(), selected))
				{
					m_type = (TonemapType)type;
				}
				if (selected)
				{
					ImGui::SetItemDefaultFocus();	// ensure keyboard/controller navigation works
				}
			}
			ImGui::EndCombo();
		}
		ImGui::End();
	}

	bool TonemapCompute::Initialise(Device& d)
	{
		R3_PROF_EVENT();

		static_assert(std::size(c_toneMapShaders) == MaxTonemapTypes);

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

		// load all the shaders
		VkShaderModule allShaders[std::size(c_toneMapShaders)];
		for (int i = 0; i < std::size(c_toneMapShaders); ++i)
		{
			allShaders[i] = VulkanHelpers::LoadShaderModule(d.GetVkDevice(), c_toneMapShaders[i]);
			if (allShaders[i] == VK_NULL_HANDLE)
			{
				LogError("Failed to load tonemapping shader{}", c_toneMapShaders[i]);
				return false;
			}
		}

		// Create the pipelines
		VkPipelineLayoutCreateInfo pipelineLayoutInfo = { 0 };
		pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutInfo.pSetLayouts = &m_descriptorLayout;
		pipelineLayoutInfo.setLayoutCount = 1;
		if (!VulkanHelpers::CheckResult(vkCreatePipelineLayout(d.GetVkDevice(), &pipelineLayoutInfo, nullptr, &m_pipelineLayout)))
		{
			LogError("Failed to create pipeline layout");
			return false;
		}

		for (int i = 0; i < std::size(m_pipelines); ++i)
		{
			m_pipelines[i] = VulkanHelpers::CreateComputePipeline(d.GetVkDevice(), allShaders[i], m_pipelineLayout, "main");
			if (m_pipelines[i] == VK_NULL_HANDLE)
			{
				LogError("Failed to create tonemap pipeline for shader {}", c_toneMapShaders[i]);
			}
		}

		for (int i = 0; i < std::size(c_toneMapShaders); ++i)
		{
			vkDestroyShaderModule(d.GetVkDevice(), allShaders[i], nullptr);
		}

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

		DescriptorSetWriter writer(m_descriptorSets[m_currentSet]);
		writer.WriteStorageImage(0, hdrTarget.m_view, hdrTarget.m_lastLayout);
		writer.WriteStorageImage(1, outputTarget.m_view, outputTarget.m_lastLayout);
		writer.FlushWrites();

		assert(m_type < MaxTonemapTypes);
		vkCmdBindPipeline(cmds, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipelines[m_type]);
		vkCmdBindDescriptorSets(cmds, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipelineLayout, 0, 1, &m_descriptorSets[m_currentSet], 0, nullptr);
		auto dimensions = glm::min(hdrDimensions, outputDimensions);

		vkCmdDispatch(cmds, (uint32_t)glm::ceil(dimensions.x / 16.0f), (uint32_t)glm::ceil(dimensions.y / 16.0f), 1);
		if (++m_currentSet >= c_maxSets)
		{
			m_currentSet = 0;
		}
	}

	void TonemapCompute::Cleanup(Device& d)
	{
		R3_PROF_EVENT();

		// cleanup the pipelines
		for (int i = 0; i < std::size(m_pipelines); ++i)
		{
			vkDestroyPipeline(d.GetVkDevice(), m_pipelines[i], nullptr);
		}
		vkDestroyPipelineLayout(d.GetVkDevice(), m_pipelineLayout, nullptr);

		// cleanup the descriptors
		vkDestroyDescriptorSetLayout(d.GetVkDevice(), m_descriptorLayout, nullptr);
		m_descriptorAllocator = {};
	}
}