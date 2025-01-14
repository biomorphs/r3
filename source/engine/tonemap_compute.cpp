#include "tonemap_compute.h"
#include "render/device.h"
#include "render/render_target_cache.h"
#include "render/vulkan_helpers.h"
#include "core/profiler.h"
#include "core/log.h"
#include <imgui.h>

namespace R3
{
	void TonemapCompute::ShowGui()
	{
		std::string_view c_toneMapTypes[] = {
			"Reinhard (Colour)",
			"Reinhard (Luminance)",
			"AGX",
			"AGX Golden",
			"AGX Punchy",
			"Uncharted 2 Filmic",
			"ACES Approx"
		};
		assert(m_type < std::size(c_toneMapTypes));
		ImGui::Begin("Tonemapper");
		if (ImGui::BeginCombo("Type", c_toneMapTypes[m_type].data()))
		{
			for (int type = 0; type < std::size(c_toneMapTypes); ++type)
			{
				bool selected = (type == m_type);
				if (ImGui::Selectable(c_toneMapTypes[type].data(), selected))
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

		auto computeShaderReinhard = VulkanHelpers::LoadShaderModule(d.GetVkDevice(), "shaders_spirv/common/tonemap_reinhard_compute.comp.spv");
		auto computeShaderReinhardLum = VulkanHelpers::LoadShaderModule(d.GetVkDevice(), "shaders_spirv/common/tonemap_reinhard_luminance_compute.comp.spv");
		auto computeShaderAGX = VulkanHelpers::LoadShaderModule(d.GetVkDevice(), "shaders_spirv/common/tonemap_agx_compute.comp.spv");
		auto computeShaderAGXGolden = VulkanHelpers::LoadShaderModule(d.GetVkDevice(), "shaders_spirv/common/tonemap_agx_golden_look_compute.comp.spv");
		auto computeShaderAGXPunchy = VulkanHelpers::LoadShaderModule(d.GetVkDevice(), "shaders_spirv/common/tonemap_agx_punchy_look_compute.comp.spv");
		auto computeShaderUncharted = VulkanHelpers::LoadShaderModule(d.GetVkDevice(), "shaders_spirv/common/tonemap_uncharted_compute.comp.spv");
		auto computeShaderACESApprox = VulkanHelpers::LoadShaderModule(d.GetVkDevice(), "shaders_spirv/common/tonemap_aces_approx_compute.comp.spv");
		if (computeShaderReinhard == VK_NULL_HANDLE || 
			computeShaderReinhardLum  == VK_NULL_HANDLE || 
			computeShaderAGX == VK_NULL_HANDLE || 
			computeShaderAGXGolden == VK_NULL_HANDLE || 
			computeShaderAGXPunchy == VK_NULL_HANDLE ||
			computeShaderUncharted == VK_NULL_HANDLE ||
			computeShaderACESApprox == VK_NULL_HANDLE)
		{
			LogError("Failed to load tonemap shaders");
			return false;
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
		m_pipelineReinhardColour = VulkanHelpers::CreateComputePipeline(d.GetVkDevice(), computeShaderReinhard, m_pipelineLayout, "main");
		m_pipelineReinhardLum = VulkanHelpers::CreateComputePipeline(d.GetVkDevice(), computeShaderReinhardLum, m_pipelineLayout, "main");
		m_pipelineAGX = VulkanHelpers::CreateComputePipeline(d.GetVkDevice(), computeShaderAGX, m_pipelineLayout, "main");
		m_pipelineAGXGolden = VulkanHelpers::CreateComputePipeline(d.GetVkDevice(), computeShaderAGXGolden, m_pipelineLayout, "main");
		m_pipelineAGXPunchy = VulkanHelpers::CreateComputePipeline(d.GetVkDevice(), computeShaderAGXPunchy, m_pipelineLayout, "main");
		m_pipelineUncharted = VulkanHelpers::CreateComputePipeline(d.GetVkDevice(), computeShaderUncharted, m_pipelineLayout, "main");
		m_pipelineACESApprox = VulkanHelpers::CreateComputePipeline(d.GetVkDevice(), computeShaderACESApprox, m_pipelineLayout, "main");

		// We don't need the shader any more
		vkDestroyShaderModule(d.GetVkDevice(), computeShaderACESApprox, nullptr);
		vkDestroyShaderModule(d.GetVkDevice(), computeShaderUncharted, nullptr);
		vkDestroyShaderModule(d.GetVkDevice(), computeShaderAGXPunchy, nullptr);
		vkDestroyShaderModule(d.GetVkDevice(), computeShaderAGXGolden, nullptr);
		vkDestroyShaderModule(d.GetVkDevice(), computeShaderAGX, nullptr);
		vkDestroyShaderModule(d.GetVkDevice(), computeShaderReinhardLum, nullptr);
		vkDestroyShaderModule(d.GetVkDevice(), computeShaderReinhard, nullptr);

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

		VkPipeline pipeline = VK_NULL_HANDLE;
		switch (m_type)
		{
		case ReinhardColour:
			pipeline = m_pipelineReinhardColour;
			break;
		case ReinhardLuminance:
			pipeline = m_pipelineReinhardLum;
			break;
		case AGX:
			pipeline = m_pipelineAGX;
			break;
		case AGXGolden:
			pipeline = m_pipelineAGXGolden;
			break;
		case AGXPunchy:
			pipeline = m_pipelineAGXPunchy;
			break;
		case Uncharted:
			pipeline = m_pipelineUncharted;
			break;
		case ACESApprox:
			pipeline = m_pipelineACESApprox;
			break;
		default:
			pipeline = m_pipelineReinhardColour;
		}

		vkCmdBindPipeline(cmds, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
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

		// cleanup the pipelines
		vkDestroyPipeline(d.GetVkDevice(), m_pipelineReinhardColour, nullptr);
		vkDestroyPipeline(d.GetVkDevice(), m_pipelineReinhardLum, nullptr);
		vkDestroyPipeline(d.GetVkDevice(), m_pipelineAGX, nullptr);
		vkDestroyPipeline(d.GetVkDevice(), m_pipelineAGXGolden, nullptr);
		vkDestroyPipeline(d.GetVkDevice(), m_pipelineAGXPunchy, nullptr);
		vkDestroyPipeline(d.GetVkDevice(), m_pipelineUncharted, nullptr);
		vkDestroyPipeline(d.GetVkDevice(), m_pipelineACESApprox, nullptr);
		vkDestroyPipelineLayout(d.GetVkDevice(), m_pipelineLayout, nullptr);

		// cleanup the descriptors
		vkDestroyDescriptorSetLayout(d.GetVkDevice(), m_descriptorLayout, nullptr);
		m_descriptorAllocator = {};
	}
}