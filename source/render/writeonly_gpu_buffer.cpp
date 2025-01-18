#include "writeonly_gpu_buffer.h"
#include "device.h"
#include "core/profiler.h"
#include "core/log.h"

namespace R3
{
	bool WriteOnlyGpuBuffer::Create(Device& d, uint64_t dataMaxSize, uint64_t stagingMaxSize, VkBufferUsageFlags usageFlags)
	{
		R3_PROF_EVENT();
		m_allData = VulkanHelpers::CreateBuffer(d.GetVMA(), dataMaxSize, usageFlags | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
		VulkanHelpers::SetBufferName(d.GetVkDevice(), m_allData, m_debugName);
		m_allDataAddress = VulkanHelpers::GetBufferDeviceAddress(d.GetVkDevice(), m_allData);
		m_allDataMaxSize = dataMaxSize;
		m_stagingBuffer = VulkanHelpers::CreateBuffer(d.GetVMA(), stagingMaxSize,
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VMA_MEMORY_USAGE_AUTO,
			VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);	// host coherant, write combined)
		VulkanHelpers::SetBufferName(d.GetVkDevice(), m_stagingBuffer, m_debugName + " (Staging)");
		m_stagingMaxSize = stagingMaxSize;
		if (!VulkanHelpers::CheckResult(vmaMapMemory(d.GetVMA(), m_stagingBuffer.m_allocation, &m_stagingMappedPtr)))
		{
			LogError("Failed to map staging buffer memory");
			return false;
		}

		return m_allData.m_allocation != VK_NULL_HANDLE && m_stagingBuffer.m_allocation != VK_NULL_HANDLE && m_stagingMappedPtr != nullptr;
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
		if (sizeBytes == 0 || writeStartOffset == -1 || (writeStartOffset + sizeBytes) > m_allDataCurrentSizeBytes.load())
		{
			LogWarn("Invalid gpu buffer write cmd");
			return false;
		}
		if (sizeBytes > m_stagingMaxSize)
		{
			LogWarn("Gpu buffer write too big for staging buffer");
			return false;
		}

		uint64_t stagingOffset = m_stagingEndOffset.fetch_add(sizeBytes);
		// if staging buffer is full, reset it
		// this is NOT safe, we need multiple staging buffers to do it properly
		if (stagingOffset == m_stagingMaxSize)
		{
			m_stagingEndOffset.store(0);
			stagingOffset = 0;
		}
		else if (stagingOffset + sizeBytes > m_stagingMaxSize)	// write over end of buffer, dangerous wraparound
		{
			m_stagingEndOffset.store(0);
			stagingOffset = 0;	
			LogWarn("Dangerous staging buffer wraparound");
		}
		if (stagingOffset + sizeBytes <= m_stagingMaxSize)
		{
			uint8_t* stagingPtr = static_cast<uint8_t*>(m_stagingMappedPtr) + stagingOffset;
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
			LogError("Write too big for gpu data buffer");
		}

		return false;
	}

	void WriteOnlyGpuBuffer::Flush(Device& d, VkCommandBuffer cmds, VkPipelineStageFlags barrierDst)
	{
		R3_PROF_EVENT();
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
			vkCmdCopyBuffer(cmds, m_stagingBuffer.m_buffer, m_allData.m_buffer, static_cast<uint32_t>(copyRegions.size()), copyRegions.data());

			// use a memory barrier to ensure the transfer finishes before any vertex reads
			// dst stage probably needs to be customisable 
			VkMemoryBarrier writeBarrier = { 0 };
			writeBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
			writeBarrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
			writeBarrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
			vkCmdPipelineBarrier(cmds,
				VK_PIPELINE_STAGE_TRANSFER_BIT,		// src stage = transfer
				barrierDst,							// dst stage = vertex shader input by default
				0,									// dependency flags
				1,
				&writeBarrier,
				0, nullptr, 0, nullptr
			);
		}
	}

	bool WriteOnlyGpuBuffer::IsCreated()
	{
		return m_allData.m_allocation != VK_NULL_HANDLE && m_stagingBuffer.m_allocation != VK_NULL_HANDLE && m_stagingMappedPtr != nullptr;
	}

	void WriteOnlyGpuBuffer::Destroy(Device& d)
	{
		R3_PROF_EVENT();
		if (m_allData.m_allocation)
		{
			vmaDestroyBuffer(d.GetVMA(), m_allData.m_buffer, m_allData.m_allocation);
		}
		if (m_stagingBuffer.m_allocation)
		{
			vmaUnmapMemory(d.GetVMA(), m_stagingBuffer.m_allocation);
			vmaDestroyBuffer(d.GetVMA(), m_stagingBuffer.m_buffer, m_stagingBuffer.m_allocation);
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
		return m_allDataAddress;
	}

	VkBuffer WriteOnlyGpuBuffer::GetBuffer()
	{
		return m_allData.m_buffer;
	}

	void WriteOnlyGpuBuffer::SetDebugName(std::string_view n)
	{
		m_debugName = n;
	}
}