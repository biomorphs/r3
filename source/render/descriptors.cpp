#include "descriptors.h"
#include "vulkan_helpers.h"
#include "device.h"
#include "render_system.h"
#include "engine/systems/time_system.h"
#include "core/log.h"
#include "core/profiler.h"

namespace R3
{
	void DescriptorLayoutBuilder::AddBinding(uint32_t binding, uint32_t count, VkDescriptorType type, VkShaderStageFlags stageFlags)
	{
		VkDescriptorSetLayoutBinding b = {};
		b.binding = binding;
		b.descriptorType = type;
		b.stageFlags = stageFlags;
		b.descriptorCount = count;
		m_bindings.push_back(b);
	}

	VkDescriptorSetLayout DescriptorLayoutBuilder::Create(Device& d, VkDescriptorSetLayoutCreateFlags flags)
	{
		R3_PROF_EVENT();
		VkDescriptorSetLayout newLayout = nullptr;
		VkDescriptorSetLayoutCreateInfo createDescLayout = {};
		createDescLayout.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		createDescLayout.flags = flags;
		createDescLayout.bindingCount = static_cast<uint32_t>(m_bindings.size());
		createDescLayout.pBindings = m_bindings.data();
		if (!VulkanHelpers::CheckResult(vkCreateDescriptorSetLayout(d.GetVkDevice(), &createDescLayout, nullptr, &newLayout)))
		{
			LogError("Failed to create descriptor set layout!");
			newLayout = nullptr;
		}
		return newLayout;
	}


	DescriptorSetSimpleAllocator::~DescriptorSetSimpleAllocator()
	{
		R3_PROF_EVENT();
		auto d = Systems::GetSystem<RenderSystem>()->GetDevice();
		vkDestroyDescriptorPool(d->GetVkDevice(), m_pool, nullptr);
	}

	bool DescriptorSetSimpleAllocator::Initialise(Device& d, uint32_t maxSets, const std::vector<VkDescriptorPoolSize>& poolSizes)
	{
		R3_PROF_EVENT();
		VkDescriptorPoolCreateInfo pool_info = { .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
		pool_info.flags = 0;
		pool_info.maxSets = maxSets;
		pool_info.poolSizeCount = (uint32_t)poolSizes.size();
		pool_info.pPoolSizes = poolSizes.data();
		if (!VulkanHelpers::CheckResult(vkCreateDescriptorPool(d.GetVkDevice(), &pool_info, nullptr, &m_pool)))
		{
			LogError("Failed to initialise descriptor set allocator");
			m_pool = nullptr;
			return false;
		}
		return true;
	}

	VkDescriptorSet DescriptorSetSimpleAllocator::Allocate(Device& d, VkDescriptorSetLayout layout)
	{
		R3_PROF_EVENT();
		VkDescriptorSet newSet;
		auto time = Systems::GetSystem<TimeSystem>();
		for (int r = 0; r < m_releasedSets.size(); ++r)	// first check released sets
		{
			if (m_releasedSets[r].m_frameReleased + c_framesBeforeRelease < time->GetFrameIndex())
			{
				if (m_releasedSets[r].m_layout == layout)
				{
					newSet = std::move(m_releasedSets[r].m_set);
					m_releasedSets.erase(m_releasedSets.begin() + r);
					return newSet;
				}
			}
		}

		VkDescriptorSetAllocateInfo allocInfo = { .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
		allocInfo.pNext = nullptr;
		allocInfo.descriptorPool = m_pool;
		allocInfo.descriptorSetCount = 1;
		allocInfo.pSetLayouts = &layout;
		if (!VulkanHelpers::CheckResult(vkAllocateDescriptorSets(d.GetVkDevice(), &allocInfo, &newSet)))
		{
			LogError("Failed to allocate descriptor set");
			newSet = {};
		}
		return newSet;
	}

	void DescriptorSetSimpleAllocator::Release(VkDescriptorSet set, VkDescriptorSetLayout layout)
	{
		R3_PROF_EVENT();
		auto time = Systems::GetSystem<TimeSystem>();
		ReleasedSet r;
		r.m_set = set;
		r.m_frameReleased = time->GetFrameIndex();
		r.m_layout = layout;
		m_releasedSets.push_back(r);
	}
}