#pragma once

#include "vulkan_helpers.h"
#include <atomic>
#include <concurrentqueue/concurrentqueue.h>

namespace R3
{
	// a gpu buffer with deferred writes
	// has a fixed max size, set via Create()
	// has a fixed max size staging buffer
	// writes are copied to a staging buffer and scheduled for flush (lock free, thread-safe)
	// flush will call vkCopyBuffer to push from staging -> the actual buffer
	class Device;
	class WriteOnlyGpuBuffer
	{
	public:
		WriteOnlyGpuBuffer() = default;
		WriteOnlyGpuBuffer(const WriteOnlyGpuBuffer&) = delete;
		WriteOnlyGpuBuffer(WriteOnlyGpuBuffer&&) = delete;
		bool Create(Device& d, uint64_t dataMaxSize, uint64_t stagingMaxSize, VkBufferUsageFlags usageFlags);	// allocates memory, needs the device
		uint64_t Allocate(uint64_t sizeBytes);									// allocate from internal data
		bool Write(uint64_t writeStartOffset, uint64_t sizeBytes, const void* data);	// writes to staging buffer + schedules copy
		void Flush(Device& d, VkCommandBuffer cmds);							// schedules all copies from staging->alldata, issues pipeline barrier
		bool IsCreated();
		void Destroy(Device& d);
		uint64_t GetSize();
		uint64_t GetMaxSize();
	private:
		AllocatedBuffer m_allData;
		VkDeviceAddress m_allDataAddress;
		uint64_t m_allDataMaxSize = 0;		// bytes
		std::atomic<uint64_t> m_allDataCurrentSizeBytes;
		AllocatedBuffer m_stagingBuffer;
		uint64_t m_stagingMaxSize = 0;		// bytes
		void* m_stagingMappedPtr = nullptr;
		std::atomic<uint64_t> m_stagingEndOffset = 0;	// bytes
		struct ScheduledWrite
		{
			uint64_t m_targetOffset;	// copy to here in m_allData (bytes)
			uint64_t m_stagingOffset;	// from here in m_stagingBuffer (bytes)
			uint64_t m_size;			// bytes to copy
		};
		moodycamel::ConcurrentQueue<ScheduledWrite> m_stagingWrites;	// all writes to flush 
	};

	// helper for storing write-only arrays on the gpu
	template<class Type>
	class WriteOnlyGpuArray
	{
	public:
		WriteOnlyGpuArray() = default;
		WriteOnlyGpuArray(const WriteOnlyGpuArray&) = delete;
		WriteOnlyGpuArray(WriteOnlyGpuArray&&) = delete;
		bool Create(Device& d, uint64_t maxStored, uint64_t maxStaging, VkBufferUsageFlags usageFlags)
		{
			return m_buffer.Create(d, maxStored * sizeof(Type), maxStaging * sizeof(Type), usageFlags);
		}
		uint64_t Allocate(uint64_t count)
		{
			return m_buffer.Allocate(count * sizeof(Type)) / sizeof(Type);
		}
		bool Write(uint64_t startIndex, uint64_t count, const Type* data)
		{
			return m_buffer.Write(startIndex * sizeof(Type), count * sizeof(Type), data);
		}
		void Flush(Device& d, VkCommandBuffer cmds)
		{
			return m_buffer.Flush(d, cmds);
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
	private:
		WriteOnlyGpuBuffer m_buffer;
	};
}