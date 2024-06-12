#include "command_buffer_allocator.h"
#include "vulkan_helpers.h"
#include "device.h"
#include "render_system.h"
#include "core/log.h"
#include "core/profiler.h"

namespace R3
{
	CommandBufferAllocator::CommandBufferAllocator()
	{
	}

	CommandBufferAllocator::~CommandBufferAllocator()
	{
		auto d = Systems::GetSystem<RenderSystem>()->GetDevice();
		Destroy(*d);
	}

	VkCommandPool_T* CommandBufferAllocator::GetPool(Device& d)
	{
		R3_PROF_EVENT();
		const auto thisThreadID = std::this_thread::get_id();
		auto pool = m_perThreadData.find(thisThreadID);
		if (pool == m_perThreadData.end())
		{
			PerThreadData newThreadData;
			auto queueFamilyIndices = VulkanHelpers::FindQueueFamilyIndices(d.GetPhysicalDevice(), d.GetMainSurface());
			VkCommandPoolCreateInfo poolInfo = {};
			poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
			poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;				// we are going to allocate/free cmd buffers a lot
			poolInfo.queueFamilyIndex = queueFamilyIndices.m_graphicsIndex;		// graphics queue pls
			if (!VulkanHelpers::CheckResult(vkCreateCommandPool(d.GetVkDevice(), &poolInfo, nullptr, &newThreadData.m_pool)))
			{
				LogError("failed to create command pool!");
				return nullptr;
			}
			m_perThreadData[thisThreadID] = newThreadData;
			pool = m_perThreadData.find(thisThreadID);
		}
		return pool->second.m_pool;
	}

	VkCommandBuffer_T* CommandBufferAllocator::CreateCommandBuffer(Device& d, bool isPrimary)
	{
		R3_PROF_EVENT();
		auto pool = GetPool(d);
		if (pool != VK_NULL_HANDLE)
		{
			VkCommandBufferAllocateInfo allocInfo = {};
			allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
			allocInfo.commandPool = pool;
			allocInfo.level = isPrimary ? VK_COMMAND_BUFFER_LEVEL_PRIMARY : VK_COMMAND_BUFFER_LEVEL_SECONDARY;
			allocInfo.commandBufferCount = 1;
			VkCommandBuffer newCmdBuffer = nullptr;;
			if (!VulkanHelpers::CheckResult(vkAllocateCommandBuffers(d.GetVkDevice(), &allocInfo, &newCmdBuffer)))
			{
				LogError("failed to allocate command buffer!");
			}
			return newCmdBuffer;
		}

		return nullptr;
	}

	void CommandBufferAllocator::Reset(Device& d)
	{
		R3_PROF_EVENT();
		// danger, if any thread is currently touching this then expect fireworks
		for (const auto& it : m_perThreadData)
		{
			if (!VulkanHelpers::CheckResult(vkResetCommandPool(d.GetVkDevice(), it.second.m_pool, 0)))
			{
				LogError("Failed to reset cmd pool!");
			}
		}
	}

	void CommandBufferAllocator::Destroy(Device& d)
	{
		R3_PROF_EVENT();
		// danger, if any thread is currently touching this then expect fireworks
		for (const auto& it : m_perThreadData)
		{
			vkDestroyCommandPool(d.GetVkDevice(), it.second.m_pool, nullptr);
		}
	}
}