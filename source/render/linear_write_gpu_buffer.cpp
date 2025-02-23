#include "linear_write_gpu_buffer.h"
#include "device.h"
#include "render_system.h"	// for staging buffer pool
#include "core/profiler.h"
#include "core/log.h"

namespace R3
{
	uint8_t* LinearWriteGpuBuffer::GetWritePtr() const
	{
		if (m_stagingBuffer.m_mappedBuffer == nullptr)
		{
			LogError("LinearWriteGpuBuffer with no staging buffer!");
			return nullptr;
		}
		return static_cast<uint8_t*>(m_stagingBuffer.m_mappedBuffer) + m_writeOffset;
	}

	void LinearWriteGpuBuffer::CommitWrites(uint32_t sizeBytes)
	{
		if (m_writeOffset + sizeBytes > m_maxSize)
		{
			LogError("Attempting to commit write too big for this buffer!");
			return;
		}
		m_writeOffset += sizeBytes;
	}
	
	bool LinearWriteGpuBuffer::Create(std::string_view name, Device& d, uint32_t dataMaxSize, VkBufferUsageFlags usageFlags)
	{
		R3_PROF_EVENT();

		// we always want transfer dst (to copy staging -> buffer) and device address bit
		m_usageFlags = usageFlags | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
		m_maxSize = dataMaxSize;
		m_debugName = name;

		if (!AcqureNewTargetBuffer(d))
		{
			LogError("Failed to get create new gpu buffer");
			return false;
		}
		if (!AcquireNewStagingBuffer(d))
		{
			LogError("Failed to acquire staging buffer");
			return false;
		}
		return m_pooledBuffer.m_buffer.m_allocation != VK_NULL_HANDLE;
	}
	
	uint32_t LinearWriteGpuBuffer::Write(uint32_t sizeBytes, const void* data)
	{
		if (m_stagingBuffer.m_buffer.m_buffer == VK_NULL_HANDLE || m_stagingBuffer.m_mappedBuffer == nullptr)
		{
			LogError("Gpu buffer {} has no staging buffer!", m_debugName);
			return false;
		}
		if (sizeBytes == 0 || (m_writeOffset + sizeBytes) > m_maxSize)
		{
			LogWarn("Invalid gpu buffer write cmd for {}", m_debugName);
			return false;
		}

		const uint32_t thisWriteOffset = m_writeOffset;
		uint8_t* stagingPtr = static_cast<uint8_t*>(m_stagingBuffer.m_mappedBuffer) + m_writeOffset;
		memcpy(stagingPtr, data, sizeBytes);
		m_writeOffset += sizeBytes;

		return thisWriteOffset;
	}
	
	void LinearWriteGpuBuffer::RetirePooledBuffer(Device& d)
	{
		R3_PROF_EVENT();

		// retire the old buffer
		auto pool = Systems::GetSystem<RenderSystem>()->GetBufferPool();
		pool->Release(m_pooledBuffer);
		m_pooledBuffer = {};

		if (!AcqureNewTargetBuffer(d))
		{
			LogError("Failed to acquire new gpu buffer");
		}
	}
	
	void LinearWriteGpuBuffer::Flush(Device& d, VkCommandBuffer cmds, VkPipelineStageFlags barrierDst)
	{
		R3_PROF_EVENT();
		if (m_writeOffset > 0)
		{
			VkBufferCopy copyRegion;
			copyRegion.srcOffset = 0;
			copyRegion.dstOffset = 0;
			copyRegion.size = m_writeOffset;

			vkCmdCopyBuffer(cmds, m_stagingBuffer.m_buffer.m_buffer, m_pooledBuffer.m_buffer.m_buffer, 1, &copyRegion);

			// use a memory barrier to ensure the transfer finishes before the next pipeline stage
			VulkanHelpers::DoMemoryBarrier(cmds, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_MEMORY_WRITE_BIT, barrierDst, VK_ACCESS_MEMORY_READ_BIT);

			if (!AcquireNewStagingBuffer(d))
			{
				LogError("Failed to acquire new staging buffer! All writes will fail");
			}
		}
	}
	
	bool LinearWriteGpuBuffer::IsCreated()
	{
		return m_pooledBuffer.m_buffer.m_allocation != VK_NULL_HANDLE;
	}
	
	void LinearWriteGpuBuffer::Destroy(Device& d)
	{
		R3_PROF_EVENT();
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
	
	VkDeviceAddress LinearWriteGpuBuffer::GetBufferDeviceAddress()
	{
		return m_pooledBuffer.m_deviceAddress;
	}
	
	VkBuffer LinearWriteGpuBuffer::GetBuffer()
	{
		return m_pooledBuffer.m_buffer.m_buffer;
	}
	
	bool LinearWriteGpuBuffer::AcquireNewStagingBuffer(Device& d)
	{
		R3_PROF_EVENT();

		m_writeOffset = 0;

		auto pool = Systems::GetSystem<RenderSystem>()->GetBufferPool();
		if (m_stagingBuffer.m_buffer.m_buffer != VK_NULL_HANDLE)	// release the old buffer
		{
			pool->Release(m_stagingBuffer);
			m_stagingBuffer = {};
		}

		auto newStagingBuffer = pool->GetBuffer(m_debugName + "_staging", m_maxSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_AUTO, true);
		if (!newStagingBuffer.has_value())
		{
			LogError("Failed to aquire new staging buffer");
			return false;
		}
		else
		{
			m_stagingBuffer = *newStagingBuffer;
			VulkanHelpers::SetBufferName(d.GetVkDevice(), m_stagingBuffer.m_buffer, m_debugName + " (Staging)");	// can we rename existing buffers?!
		}

		return true;
	}

	bool LinearWriteGpuBuffer::AcqureNewTargetBuffer(Device& d)
	{
		R3_PROF_EVENT();
		auto pool = Systems::GetSystem<RenderSystem>()->GetBufferPool();
		auto newBuffer = pool->GetBuffer(m_debugName, m_maxSize, m_usageFlags, VMA_MEMORY_USAGE_AUTO, false);
		if (!newBuffer)
		{
			LogError("Failed to get buffer from pool");
			return false;
		}
		VulkanHelpers::SetBufferName(d.GetVkDevice(), newBuffer->m_buffer, m_debugName);
		m_pooledBuffer = *newBuffer;
		return true;
	}
}