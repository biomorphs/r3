#include "descriptors.h"
#include "vulkan_helpers.h"
#include "device.h"
#include "render_system.h"
#include "engine/systems/time_system.h"
#include "core/log.h"
#include "core/profiler.h"

namespace R3
{
	DescriptorSetWriter::DescriptorSetWriter(VkDescriptorSet targetSet)
		: m_target(targetSet)
	{
	}

	void DescriptorSetWriter::FlushWrites()
	{
		R3_PROF_EVENT();
		if (m_writes.size() > 0)
		{
			auto render = Systems::GetSystem<RenderSystem>();
			vkUpdateDescriptorSets(render->GetDevice()->GetVkDevice(), (uint32_t)m_writes.size(), m_writes.data(), 0, nullptr);
		}
	}

	void DescriptorSetWriter::WriteStorageBuffer(uint32_t binding, VkBuffer buffer, uint64_t bufferOffset, uint64_t bufferSize)
	{
		VkDescriptorBufferInfo bufferInfo = {};
		bufferInfo.buffer = buffer;
		bufferInfo.offset = bufferOffset;
		bufferInfo.range = bufferSize == 0 ? VK_WHOLE_SIZE : bufferSize;
		m_bufferInfos.push_back(bufferInfo);

		VkWriteDescriptorSet writeTextures = {};
		writeTextures.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeTextures.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		writeTextures.descriptorCount = 1;
		writeTextures.dstArrayElement = 0;
		writeTextures.dstBinding = binding;
		writeTextures.dstSet = m_target;
		writeTextures.pBufferInfo = &m_bufferInfos[m_bufferInfos.size() - 1];
		m_writes.push_back(writeTextures);
	}

	void DescriptorSetWriter::WriteUniformBuffer(uint32_t binding, VkBuffer buffer, uint64_t bufferOffset, uint64_t bufferSize)
	{
		VkDescriptorBufferInfo bufferInfo = {};
		bufferInfo.buffer = buffer;
		bufferInfo.offset = bufferOffset;
		bufferInfo.range = bufferSize == 0 ? VK_WHOLE_SIZE : bufferSize;
		m_bufferInfos.push_back(bufferInfo);

		VkWriteDescriptorSet writeTextures = {};
		writeTextures.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeTextures.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		writeTextures.descriptorCount = 1;
		writeTextures.dstArrayElement = 0;
		writeTextures.dstBinding = binding;
		writeTextures.dstSet = m_target;
		writeTextures.pBufferInfo = &m_bufferInfos[m_bufferInfos.size()-1];
		m_writes.push_back(writeTextures);
	}

	void DescriptorSetWriter::WriteImage(uint32_t binding, uint32_t arrayIndex, VkImageView view, VkSampler sampler, VkImageLayout layout)
	{
		VkDescriptorImageInfo newImgInfo = {};
		newImgInfo.imageLayout = layout;
		newImgInfo.imageView = view;	
		newImgInfo.sampler = sampler;
		m_imgInfos.push_back(newImgInfo);

		VkWriteDescriptorSet writeTextures = {};
		writeTextures.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeTextures.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		writeTextures.descriptorCount = 1;
		writeTextures.dstArrayElement = arrayIndex;
		writeTextures.dstBinding = binding;
		writeTextures.dstSet = m_target;
		writeTextures.pImageInfo = &m_imgInfos[m_imgInfos.size() - 1];
		m_writes.push_back(writeTextures);
	}

	void DescriptorLayoutBuilder::AddBinding(uint32_t binding, uint32_t count, VkDescriptorType type, VkShaderStageFlags stageFlags)
	{
		VkDescriptorSetLayoutBinding b = {};
		b.binding = binding;
		b.descriptorType = type;
		b.stageFlags = stageFlags;
		b.descriptorCount = count;
		m_bindings.push_back(b);
	}

	VkDescriptorSetLayout DescriptorLayoutBuilder::Create(Device& d, bool enableBindless)
	{
		R3_PROF_EVENT();
		VkDescriptorSetLayout newLayout = nullptr;
		VkDescriptorSetLayoutCreateInfo createDescLayout = {};
		VkDescriptorSetLayoutBindingFlagsCreateInfoEXT bindingFlagInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO_EXT, nullptr };
		createDescLayout.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		createDescLayout.flags = enableBindless ? VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT_EXT : 0;
		createDescLayout.bindingCount = static_cast<uint32_t>(m_bindings.size());
		createDescLayout.pBindings = m_bindings.data();

		VkDescriptorBindingFlags bindingFlags = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT_EXT		// enable sparse writes
			| VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT_EXT;							// enable update after bind
			// | VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT_EXT					// enable variable size arrays
		if (enableBindless)
		{
			bindingFlagInfo.bindingCount = static_cast<uint32_t>(m_bindings.size());
			bindingFlagInfo.pBindingFlags = &bindingFlags;
			createDescLayout.pNext = &bindingFlagInfo;
		}

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
		pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT_EXT;	// allow for update of a set after it has been bound (for bindless)
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