#include "writeonly_gpu_buffer.h"
#include "device.h"
#include "render_system.h"	// for staging buffer pool
#include "core/profiler.h"
#include "core/log.h"

namespace R3
{
	void WriteOnlyGpuBuffer::RetirePooledBuffer(Device& d)
	{
		R3_PROF_EVENT();

		auto pool = Systems::GetSystem<RenderSystem>()->GetBufferPool();

		// retire the old buffer
		pool->Release(m_pooledBuffer);
		m_pooledBuffer = {};

		auto newBuffer = pool->GetBuffer(m_debugName, m_allDataMaxSize, m_usageFlags, VMA_MEMORY_USAGE_AUTO, false);
		if (!newBuffer)
		{
			LogError("Failed to get buffer from pool");
			return;
		}
		VulkanHelpers::SetBufferName(d.GetVkDevice(), newBuffer->m_buffer, m_debugName);
		m_pooledBuffer = *newBuffer;
	}

	bool WriteOnlyGpuBuffer::AcquireNewStagingBuffer()
	{
		R3_PROF_EVENT();

		assert(m_stagingBuffer.m_buffer.m_buffer == VK_NULL_HANDLE && m_stagingEndOffset == 0);	// make sure there is no existing staging data
		
		auto stagingPool = Systems::GetSystem<RenderSystem>()->GetBufferPool();
		auto newStagingBuffer = stagingPool->GetBuffer(m_debugName + "_staging", m_stagingMaxSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_AUTO, true);
		if (!newStagingBuffer.has_value())
		{
			LogError("Failed to aquire new staging buffer");
			return false;
		}
		else
		{
			m_stagingBuffer = *newStagingBuffer;
		}

		return true;
	}

	bool WriteOnlyGpuBuffer::Create(std::string_view name, Device& d, uint64_t dataMaxSize, uint64_t stagingMaxSize, VkBufferUsageFlags usageFlags)
	{
		R3_PROF_EVENT();

		auto pool = Systems::GetSystem<RenderSystem>()->GetBufferPool();
		m_usageFlags = usageFlags | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
		m_debugName = name; 

		ScopedLock doLock(m_mutex);

		// we always want transfer dst (to copy staging -> buffer) and device address bit
		auto newBuffer = pool->GetBuffer(m_debugName, dataMaxSize, m_usageFlags, VMA_MEMORY_USAGE_AUTO, false);
		if (!newBuffer)
		{
			LogError("Failed to get buffer from pool");
			return false;
		}
		VulkanHelpers::SetBufferName(d.GetVkDevice(), newBuffer->m_buffer, m_debugName);
		m_pooledBuffer = *newBuffer;
		
		m_allDataMaxSize = dataMaxSize;
		m_stagingMaxSize = stagingMaxSize;

		return m_pooledBuffer.m_buffer.m_allocation != VK_NULL_HANDLE;
	}

	uint64_t WriteOnlyGpuBuffer::Allocate(uint64_t sizeBytes)
	{
		R3_PROF_EVENT();
		if (sizeBytes <= m_allDataMaxSize)
		{
			uint64_t newDataOffset = m_allDataCurrentSizeBytes.fetch_add(sizeBytes);
			if ((newDataOffset + sizeBytes) <= m_allDataMaxSize)
			{
				return newDataOffset;
			}
		}
		LogWarn("Failed to allocate space in gpu buffer");
		return -1;
	}

	bool WriteOnlyGpuBuffer::Write(uint64_t writeStartOffset, uint64_t sizeBytes, const void* data)
	{
		R3_PROF_EVENT();

		ScopedLock doLock(m_mutex);

		if (m_stagingBuffer.m_buffer.m_buffer == VK_NULL_HANDLE || m_stagingBuffer.m_mappedBuffer == nullptr)
		{
			if (!AcquireNewStagingBuffer())
			{
				return false;
			}
		}
		if (sizeBytes == 0 || writeStartOffset == -1 || (writeStartOffset + sizeBytes) > m_allDataCurrentSizeBytes.load())
		{
			LogWarn("Invalid gpu buffer write cmd for {}", m_debugName);
			return false;
		}
		if (sizeBytes > m_stagingMaxSize)
		{
			LogWarn("Gpu buffer {} write too big for staging buffer", m_debugName);
			return false;
		}

		uint64_t stagingOffset = m_stagingEndOffset.fetch_add(sizeBytes);
		if (stagingOffset + sizeBytes > m_stagingMaxSize)	// staging buffer too small
		{
			LogError("Write-only gpu buffer {} staging size too small!", m_debugName);
			return false;
		}
		if (stagingOffset + sizeBytes <= m_stagingMaxSize)
		{
			uint8_t* stagingPtr = static_cast<uint8_t*>(m_stagingBuffer.m_mappedBuffer) + stagingOffset;
			memcpy(stagingPtr, data, sizeBytes);

			ScheduledWrite newWrite;
			newWrite.m_stagingOffset = stagingOffset;
			newWrite.m_size = sizeBytes;
			newWrite.m_targetOffset = writeStartOffset;
			m_stagingWrites.enqueue(newWrite);
			return true;
		}
		else
		{
			LogError("Write too big for gpu data buffer {}", m_debugName);
		}

		return false;
	}

	void WriteOnlyGpuBuffer::Flush(Device& d, VkCommandBuffer cmds, VkPipelineStageFlags barrierDst)
	{
		R3_PROF_EVENT();
		ScopedLock doLock(m_mutex);
		ScheduledWrite writeToFlush;
		int copiesIssued = 0;
		std::vector<VkBufferCopy> copyRegions;
		copyRegions.reserve(m_stagingWrites.size_approx());
		VkBufferCopy coalescedCopy = {};
		while (m_stagingWrites.try_dequeue(writeToFlush))
		{
			// if this can be coalesced, add it to the coalesced write and continue
			if (writeToFlush.m_stagingOffset == (coalescedCopy.srcOffset + coalescedCopy.size) && writeToFlush.m_targetOffset == (coalescedCopy.dstOffset + coalescedCopy.size))
			{
				coalescedCopy.size += writeToFlush.m_size;
			}
			else
			{
				if (coalescedCopy.size > 0)	// make sure to add the coalesced writes from before
				{
					copyRegions.push_back(coalescedCopy);
				}
				coalescedCopy.srcOffset = writeToFlush.m_stagingOffset;	// continue from this region
				coalescedCopy.dstOffset = writeToFlush.m_targetOffset;
				coalescedCopy.size = writeToFlush.m_size;
			}
		}
		if (coalescedCopy.size > 0)	// make sure to add the last coalesced write
		{
			copyRegions.push_back(coalescedCopy);
		}
		if (copyRegions.size() > 0)
		{
			vkCmdCopyBuffer(cmds, m_stagingBuffer.m_buffer.m_buffer, m_pooledBuffer.m_buffer.m_buffer, static_cast<uint32_t>(copyRegions.size()), copyRegions.data());

			// use a memory barrier to ensure the transfer finishes before any reads
			VulkanHelpers::DoMemoryBarrier(cmds, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_MEMORY_WRITE_BIT, barrierDst, VK_ACCESS_MEMORY_READ_BIT);

			// now release the staging buffer
			m_stagingEndOffset.store(0);
			if (m_stagingBuffer.m_buffer.m_buffer != VK_NULL_HANDLE)
			{
				auto stagingPool = Systems::GetSystem<RenderSystem>()->GetBufferPool();
				stagingPool->Release(m_stagingBuffer);
				m_stagingBuffer = {};
			}
		}
	}

	bool WriteOnlyGpuBuffer::IsCreated()
	{
		return m_pooledBuffer.m_buffer.m_allocation != VK_NULL_HANDLE;
	}

	void WriteOnlyGpuBuffer::Destroy(Device& d)
	{
		R3_PROF_EVENT();
		ScopedLock doLock(m_mutex);
		auto pool = Systems::GetSystem<RenderSystem>()->GetBufferPool();
		if (m_pooledBuffer.m_buffer.m_allocation)
		{
			pool->Release(m_pooledBuffer);
			m_pooledBuffer = {};
		}
		if (m_stagingBuffer.m_buffer.m_buffer != VK_NULL_HANDLE)
		{
			pool->Release(m_stagingBuffer);
			m_stagingBuffer = {};
		}
	}

	uint64_t WriteOnlyGpuBuffer::GetSize()
	{
		return m_allDataCurrentSizeBytes.load();
	}

	uint64_t WriteOnlyGpuBuffer::GetMaxSize()
	{
		return m_allDataMaxSize;
	}

	VkDeviceAddress WriteOnlyGpuBuffer::GetDataDeviceAddress()
	{
		return m_pooledBuffer.m_deviceAddress;
	}

	VkBuffer WriteOnlyGpuBuffer::GetBuffer()
	{
		return m_pooledBuffer.m_buffer.m_buffer;
	}
}