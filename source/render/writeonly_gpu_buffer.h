#pragma once

#include "core/mutex.h"
#include "vulkan_helpers.h"
#include "buffer_pool.h"
#include <atomic>
#include <concurrentqueue/concurrentqueue.h>

namespace R3
{
	// a gpu buffer with deferred writes
	// has a fixed max size, set via Create()
	// fixed-sized staging buffers provided by render system buffer pool (acquired on write, released on flush)
	// writes are copied to staging buffer and scheduled for flush (lock free, thread-safe)
	// flush will call vkCopyBuffer to push from staging -> the actual buffer, release the old staging buffer
	// async write/flush guarded by a mutex
	// use Write() to schedule individual buffer copies (with write combining)
	class Device;
	class WriteOnlyGpuBuffer
	{
	public:
		WriteOnlyGpuBuffer() = default;
		WriteOnlyGpuBuffer(const WriteOnlyGpuBuffer&) = delete;
		WriteOnlyGpuBuffer(WriteOnlyGpuBuffer&&) = delete;
		void RetirePooledBuffer(Device&);			// old buffer is released + a new one is created. call this before flushing writes
		bool Create(std::string_view name, Device& d, uint64_t dataMaxSize, uint64_t stagingMaxSize, VkBufferUsageFlags usageFlags);	// allocates memory
		uint64_t Allocate(uint64_t sizeBytes);									// allocate from internal data
		bool Write(uint64_t writeStartOffset, uint64_t sizeBytes, const void* data);	// writes to staging buffer + schedules copy
		void Flush(Device& d, VkCommandBuffer cmds, VkPipelineStageFlags barrierDst = VK_PIPELINE_STAGE_VERTEX_INPUT_BIT);	// schedules all copies from staging->alldata, issues pipeline barrier
		bool IsCreated();
		void Destroy(Device& d);
		uint64_t GetSize();
		uint64_t GetMaxSize();
		VkDeviceAddress GetDataDeviceAddress();
		VkBuffer GetBuffer();
	private:
		Mutex m_mutex;	// required since write and flush cannot overlap
		bool AcquireNewStagingBuffer();
		
		// keep track of flags used during creation
		VkBufferUsageFlags m_usageFlags;

		// pooled buffer data
		PooledBuffer m_pooledBuffer;

		uint64_t m_allDataMaxSize = 0;		// bytes
		std::atomic<uint64_t> m_allDataCurrentSizeBytes;
		PooledBuffer m_stagingBuffer;
		uint64_t m_stagingMaxSize = 0;		// bytes
		std::atomic<uint64_t> m_stagingEndOffset = 0;	// bytes
		struct ScheduledWrite
		{
			uint64_t m_targetOffset;	// copy to here in m_allData (bytes)
			uint64_t m_stagingOffset;	// from here in m_stagingBuffer (bytes)
			uint64_t m_size;			// bytes to copy
		};
		moodycamel::ConcurrentQueue<ScheduledWrite> m_stagingWrites;	// all writes to flush 
		std::string m_debugName;
	};

	// helper for storing write-only arrays on the gpu
	template<class Type>
	class WriteOnlyGpuArray
	{
	public:
		WriteOnlyGpuArray() = default;
		WriteOnlyGpuArray(const WriteOnlyGpuArray&) = delete;
		WriteOnlyGpuArray(WriteOnlyGpuArray&&) = delete;
		bool Create(std::string_view name, Device& d, uint64_t maxStored, uint64_t maxStaging, VkBufferUsageFlags usageFlags)
		{
			return m_buffer.Create(name, d, maxStored * sizeof(Type), maxStaging * sizeof(Type), usageFlags);
		}
		void RetirePooledBuffer(Device& d)	// releases old target buffer, acquires a new one.
		{
			m_buffer.RetirePooledBuffer(d);
		}
		uint64_t Allocate(uint64_t count)
		{
			return m_buffer.Allocate(count * sizeof(Type)) / sizeof(Type);
		}
		bool Write(uint64_t startIndex, uint64_t count, const Type* data)
		{
			return m_buffer.Write(startIndex * sizeof(Type), count * sizeof(Type), data);
		}
		void Flush(Device& d, VkCommandBuffer cmds, VkPipelineStageFlags barrierDst = VK_PIPELINE_STAGE_VERTEX_INPUT_BIT)
		{
			return m_buffer.Flush(d, cmds, barrierDst);
		}
		bool IsCreated()
		{
			return m_buffer.IsCreated();
		}
		void Destroy(Device& d)
		{
			m_buffer.Destroy(d);
		}
		uint64_t GetAllocated()
		{
			return m_buffer.GetSize() / sizeof(Type);
		}
		uint64_t GetMaxAllocated()
		{
			return m_buffer.GetMaxSize() / sizeof(Type);
		}
		VkDeviceAddress GetDataDeviceAddress()
		{
			return m_buffer.GetDataDeviceAddress();
		}
		VkBuffer GetBuffer()
		{
			return m_buffer.GetBuffer();
		}
	private:
		WriteOnlyGpuBuffer m_buffer;
	};
}