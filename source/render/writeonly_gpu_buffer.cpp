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
		m_allDataAddress = VulkanHelpers::GetBufferDeviceAddress(d.GetVkDevice(), m_allData);
		m_allDataMaxSize = dataMaxSize;
		m_stagingBuffer = VulkanHelpers::CreateBuffer(d.GetVMA(), stagingMaxSize,
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VMA_MEMORY_USAGE_AUTO,
			VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);	// host coherant, write combined)
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
		if (sizeBytes < m_allDataMaxSize)
		{
			uint64_t newDataOffset = m_allDataCurrentSizeBytes.fetch_add(sizeBytes);
			if ((newDataOffset + sizeBytes) < m_allDataMaxSize)
			{
				return newDataOffset;
			}
		}
		return -1;
	}

	bool WriteOnlyGpuBuffer::Write(uint64_t writeStartOffset, uint64_t sizeBytes, const void* data)
	{
		R3_PROF_EVENT();
		if (writeStartOffset == -1 || (writeStartOffset + sizeBytes) > m_allDataCurrentSizeBytes.load())
		{
			return false;
		}

		uint64_t stagingOffset = m_stagingEndOffset.fetch_add(sizeBytes);
		if (stagingOffset + sizeBytes < m_stagingMaxSize)
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

		return false;
	}

	void WriteOnlyGpuBuffer::Flush(Device& d, VkCommandBuffer cmds)
	{
		R3_PROF_EVENT();
		ScheduledWrite writeToFlush;
		int copiesIssued = 0;
		while (m_stagingWrites.try_dequeue(writeToFlush))
		{
			VkBufferCopy copyRegion{};
			copyRegion.srcOffset = writeToFlush.m_stagingOffset;
			copyRegion.dstOffset = writeToFlush.m_targetOffset;
			copyRegion.size = writeToFlush.m_size;
			vkCmdCopyBuffer(cmds, m_stagingBuffer.m_buffer, m_allData.m_buffer, 1, &copyRegion);
			++copiesIssued;
		}
		if (copiesIssued > 0)
		{
			// use a memory barrier to ensure the transfer finishes before any vertex reads
			// dst stage probably needs to be customisable 
			VkMemoryBarrier writeBarrier = { 0 };
			writeBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
			writeBarrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
			writeBarrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
			vkCmdPipelineBarrier(cmds,
				VK_PIPELINE_STAGE_TRANSFER_BIT,		// src stage = transfer
				VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,	// dst stage = vertex shader input
				0,									// dependency flags
				1,
				&writeBarrier,
				0, nullptr, 0, nullptr
			);

			// This is not safe, its possible to write new stuff to the staging buffer while these copies are still pending
			// we need multiple staging buffers to be safe, or a fence before writing (which would be crap)
			// lets see how long it takes to explode
			// todo double/triple buffer
			m_stagingEndOffset.store(0);
		}
	}

	bool WriteOnlyGpuBuffer::IsCreated()
	{
		return m_allData.m_allocation != VK_NULL_HANDLE && m_stagingBuffer.m_allocation != VK_NULL_HANDLE && m_stagingMappedPtr != nullptr;
	}

	void WriteOnlyGpuBuffer::Destroy(Device& d)
	{
		R3_PROF_EVENT();
		vmaUnmapMemory(d.GetVMA(), m_stagingBuffer.m_allocation);
		vmaDestroyBuffer(d.GetVMA(), m_allData.m_buffer, m_allData.m_allocation);
		vmaDestroyBuffer(d.GetVMA(), m_stagingBuffer.m_buffer, m_stagingBuffer.m_allocation);
	}

	uint64_t WriteOnlyGpuBuffer::GetSize()
	{
		return m_allDataCurrentSizeBytes.load();
	}

	uint64_t WriteOnlyGpuBuffer::GetMaxSize()
	{
		return m_allDataMaxSize;
	}
}