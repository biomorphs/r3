#include "buffer_pool.h"
#include "engine/systems/time_system.h"
#include "render/render_system.h"
#include "render/device.h"
#include "core/log.h"
#include "core/profiler.h"

namespace R3
{
	BufferPool::BufferPool(std::string_view name, uint64_t totalBudget)
		: m_totalBudget(totalBudget)
		, m_debugName(name)
	{

	}

	BufferPool::~BufferPool()
	{
		auto d = Systems::GetSystem<RenderSystem>()->GetDevice();

		ScopedLock lockReleased(m_releasedBuffersMutex);
		for (int r = 0; r < m_releasedBuffers.size(); ++r)
		{
			auto& buf = m_releasedBuffers[r];
			if (buf.m_pooledBuffer.m_mappedBuffer != nullptr)
			{
				vmaUnmapMemory(d->GetVMA(), buf.m_pooledBuffer.m_buffer.m_allocation);
			}
			vmaDestroyBuffer(d->GetVMA(), buf.m_pooledBuffer.m_buffer.m_buffer, buf.m_pooledBuffer.m_buffer.m_allocation);
		}
	}

	std::optional<PooledBuffer> BufferPool::GetBuffer(uint64_t minSizeBytes, VkBufferUsageFlags usage, VmaMemoryUsage memUsage, bool allowMapping)
	{
		R3_PROF_EVENT();
		auto time = Systems::GetSystem<TimeSystem>();
		auto d = Systems::GetSystem<RenderSystem>()->GetDevice();
		const uint64_t currentFrame = time->GetFrameIndex();
		CollectGarbage(currentFrame, *d);
		usage |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
		PooledBuffer newBuffer;
		{	// Find a usable matching buffer
			ScopedLock lockReleased(m_releasedBuffersMutex);
			for (int r = 0; r < m_releasedBuffers.size(); ++r)
			{
				auto& buf = m_releasedBuffers[r];
				if (buf.m_frameReleased + c_framesBeforeAvailable < currentFrame)
				{
					bool mappingMatch = allowMapping ? buf.m_pooledBuffer.m_mappedBuffer != nullptr : true;
					if (buf.m_pooledBuffer.m_memUsage == memUsage && buf.m_pooledBuffer.m_usage == usage && buf.m_pooledBuffer.sizeBytes >= minSizeBytes && mappingMatch)
					{
						newBuffer = std::move(buf.m_pooledBuffer);
						m_releasedBuffers.erase(m_releasedBuffers.begin() + r);
						return newBuffer;
					}
				}
			}
		}

		// create a new buffer, no existing ones can be used
		newBuffer.m_buffer = VulkanHelpers::CreateBuffer(d->GetVMA(), minSizeBytes,
			usage,
			memUsage,
			allowMapping ? VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT : 0);
		VulkanHelpers::SetBufferName(d->GetVkDevice(), newBuffer.m_buffer, m_debugName);
		if (newBuffer.m_buffer.m_buffer == VK_NULL_HANDLE)
		{
			LogWarn("Failed to create buffer of size {}", minSizeBytes);
			return {};
		}
		if (allowMapping && !VulkanHelpers::CheckResult(vmaMapMemory(d->GetVMA(), newBuffer.m_buffer.m_allocation, &newBuffer.m_mappedBuffer)))
		{
			LogError("Failed to map staging buffer memory");
			return {};
		}
		newBuffer.m_deviceAddress = VulkanHelpers::GetBufferDeviceAddress(d->GetVkDevice(), newBuffer.m_buffer);
		newBuffer.m_memUsage = memUsage;
		newBuffer.m_usage = usage;
		newBuffer.sizeBytes = minSizeBytes;
		return newBuffer;
	}

	void BufferPool::CollectGarbage(uint64_t frameIndex, class Device& d)
	{
		R3_PROF_EVENT();
		uint64_t totalBytes = 0;
		{
			ScopedLock lockReleased(m_releasedBuffersMutex);
			for (int r = 0; r < m_releasedBuffers.size(); ++r)
			{
				auto& buf = m_releasedBuffers[r];
				totalBytes += buf.m_pooledBuffer.sizeBytes;
				if (totalBytes > m_totalBudget && buf.m_frameReleased + c_framesBeforeAvailable < frameIndex)
				{
					if (buf.m_pooledBuffer.m_mappedBuffer != nullptr)
					{
						vmaUnmapMemory(d.GetVMA(), buf.m_pooledBuffer.m_buffer.m_allocation);
					}
					vmaDestroyBuffer(d.GetVMA(), buf.m_pooledBuffer.m_buffer.m_buffer, buf.m_pooledBuffer.m_buffer.m_allocation);
					m_releasedBuffers.erase(m_releasedBuffers.begin() + r);
					return;
				}
			}
		}
	}

	void BufferPool::Release(const PooledBuffer& buf)
	{
		R3_PROF_EVENT();

		auto time = Systems::GetSystem<TimeSystem>();
		ReleasedBuffer r;
		r.m_frameReleased = time->GetFrameIndex();
		r.m_pooledBuffer = buf;
		{
			ScopedLock lockReleased(m_releasedBuffersMutex);
			m_releasedBuffers.push_back(r);
		}
	}
}