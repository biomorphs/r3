#include "command_buffer_allocator.h"
#include "vulkan_helpers.h"
#include "device.h"
#include "render_system.h"
#include "engine/systems/time_system.h"
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
		ScopedLock lock(m_perThreadDataMutex);
		auto pool = m_perThreadData.find(thisThreadID);
		if (pool == m_perThreadData.end())
		{
			PerThreadData newThreadData;
			auto queueFamilyIndices = VulkanHelpers::FindQueueFamilyIndices(d.GetPhysicalDevice(), d.GetMainSurface());
			VkCommandPoolCreateInfo poolInfo = {};
			poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
			poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;	// we want to reset invdividual bufferss
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

	std::optional<ManagedCommandBuffer> CommandBufferAllocator::FindAvailableCommandBuffer(std::thread::id tId, bool isPrimary)
	{
		R3_PROF_EVENT();

		auto time = Systems::GetSystem<TimeSystem>();
		uint64_t currentFrame = time->GetFrameIndex();
		ScopedLock lock(m_releasedBuffersMutex);
		for (int i = 0; i < m_releasedBuffers.size(); ++i)
		{
			auto& r = m_releasedBuffers[i];
			if ((r.m_ownerThread == tId) && (r.m_isPrimary == isPrimary) && (r.m_frameReleased + c_framesBeforeReuse < currentFrame) )
			{
				ManagedCommandBuffer newBuffer;
				newBuffer.m_cmdBuffer = r.m_buffer;
				newBuffer.m_isPrimary = isPrimary;
				newBuffer.m_ownerThread = tId;
				m_releasedBuffers.erase(m_releasedBuffers.begin() + i);
				if (!VulkanHelpers::CheckResult(vkResetCommandBuffer(newBuffer.m_cmdBuffer, 0)))
				{
					LogError("Failed to reset released command buffer!");
					return {};	// bail
				}
				return newBuffer;
			}
		}
		return {};
	}

	std::optional<ManagedCommandBuffer> CommandBufferAllocator::CreateCommandBuffer(Device& d, bool isPrimary, std::string_view name)
	{
		R3_PROF_EVENT();

		// first check the free-list for anything
		const auto thisThreadID = std::this_thread::get_id();
		auto foundBuffer = FindAvailableCommandBuffer(thisThreadID, isPrimary);
		if (foundBuffer)
		{
			return foundBuffer;
		}
		// nope, need to allocate
		auto pool = GetPool(d);
		if (pool != VK_NULL_HANDLE)
		{
			VkCommandBufferAllocateInfo allocInfo = {};
			allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
			allocInfo.commandPool = pool;
			allocInfo.level = isPrimary ? VK_COMMAND_BUFFER_LEVEL_PRIMARY : VK_COMMAND_BUFFER_LEVEL_SECONDARY;
			allocInfo.commandBufferCount = 1;
			ManagedCommandBuffer newBuffer;
			if (!VulkanHelpers::CheckResult(vkAllocateCommandBuffers(d.GetVkDevice(), &allocInfo, &newBuffer.m_cmdBuffer)))
			{
				LogError("failed to allocate command buffer!");
				return {};
			}
			VulkanHelpers::SetCommandBufferName(d.GetVkDevice(), newBuffer.m_cmdBuffer, name);
			newBuffer.m_ownerThread = std::this_thread::get_id();
			newBuffer.m_isPrimary = isPrimary;
			return newBuffer;
		}
		return {};
	}

	void CommandBufferAllocator::Release(ManagedCommandBuffer cmdBuffer)
	{
		R3_PROF_EVENT();
		auto time = Systems::GetSystem<TimeSystem>();
		ScopedLock lock(m_releasedBuffersMutex);
		ReleasedBuffer r;
		r.m_buffer = cmdBuffer.m_cmdBuffer;
		r.m_isPrimary = cmdBuffer.m_isPrimary;
		r.m_frameReleased = time->GetFrameIndex();
		r.m_ownerThread = cmdBuffer.m_ownerThread;
		m_releasedBuffers.push_back(r);
	}

	void CommandBufferAllocator::Destroy(Device& d)
	{
		R3_PROF_EVENT();
		ScopedLock lock(m_perThreadDataMutex);
		// danger, if any thread is currently touching this then expect fireworks
		for (const auto& it : m_perThreadData)
		{
			vkDestroyCommandPool(d.GetVkDevice(), it.second.m_pool, nullptr);
		}
	}
}