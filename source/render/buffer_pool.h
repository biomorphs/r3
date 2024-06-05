#pragma once
#include "vulkan_helpers.h"
#include "core/mutex.h"
#include <optional>

namespace R3
{
	struct PooledBuffer
	{
		AllocatedBuffer m_buffer;
		uint64_t sizeBytes = 0;
		VkBufferUsageFlags m_usage = 0;
		VmaMemoryUsage m_memUsage = VMA_MEMORY_USAGE_UNKNOWN;
		void* m_mappedBuffer = 0;
	};

	// Works as an allocator of VkBuffer
	// Freed buffers are kept around until a max footprint is hit (mem budget)
	//	free takes a few frames to ensure the released buffers are not being used by the gpu
	class BufferPool
	{
	public:
		BufferPool(uint64_t totalBudget = 1024 * 1024 * 128);
		~BufferPool();

		std::optional<PooledBuffer> GetBuffer(uint64_t minSizeBytes,VkBufferUsageFlags usage,VmaMemoryUsage memUsage);
		void Release(const PooledBuffer& buf);

	private:
		void CollectGarbage(uint64_t frameIndex, class Device& d);
		Mutex m_releasedBuffersMutex;
		struct ReleasedBuffer {
			PooledBuffer m_pooledBuffer;
			uint64_t m_frameReleased = -1;
		};
		std::vector<ReleasedBuffer> m_releasedBuffers;
		const uint64_t c_framesBeforeAvailable = 6;
		uint64_t m_totalBudget = 0;
	};
}