#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include <deque>

namespace R3
{
	class Device;
	// Descriptor layouts, sets, pools - They are all annoying as hell

	// Use this to build an object describing the layout of a single descriptor set
	class DescriptorLayoutBuilder
	{
	public:
		void AddBinding(uint32_t binding, uint32_t count, VkDescriptorType type, VkShaderStageFlags stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT);
		VkDescriptorSetLayout Create(Device& d, bool enableBindless);

	private:
		std::vector<VkDescriptorSetLayoutBinding> m_bindings;
	};

	// Use this to allocate descriptor sets
	// Note allocators can handle differing layouts (but you might not want to do that)
	// this pool cannot grow
	class DescriptorSetSimpleAllocator
	{
	public:
		DescriptorSetSimpleAllocator() = default;
		~DescriptorSetSimpleAllocator();
		
		bool Initialise(Device& d, uint32_t maxSets, const std::vector<VkDescriptorPoolSize>& poolSizes);
		VkDescriptorSet Allocate(Device& d, VkDescriptorSetLayout layout);
		void Release(VkDescriptorSet set, VkDescriptorSetLayout layout);

	private:
		VkDescriptorPool m_pool;
		struct ReleasedSet
		{
			VkDescriptorSet m_set;
			VkDescriptorSetLayout m_layout;
			uint64_t m_frameReleased = -1;
		};
		std::vector<ReleasedSet> m_releasedSets;
		const uint64_t c_framesBeforeRelease = 5;
	};

	// Use this to write to descriptor sets
	class DescriptorSetWriter
	{
	public:
		DescriptorSetWriter(VkDescriptorSet targetSet);
		void WriteImage(uint32_t binding, uint32_t arrayIndex, VkImageView view, VkSampler sampler, VkImageLayout layout);
		void WriteUniformBuffer(uint32_t binding, VkBuffer buffer, uint64_t bufferOffset = 0, uint64_t bufferSize = 0);
		void FlushWrites();

	private:
		std::deque<VkDescriptorImageInfo> m_imgInfos;	// deque so ptrs are not invalidated when the container grows
		std::deque<VkDescriptorBufferInfo> m_bufferInfos;
		std::vector<VkWriteDescriptorSet> m_writes;		// all writes to push
		VkDescriptorSet m_target;
	};
}