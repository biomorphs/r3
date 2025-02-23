#pragma once
#include "vulkan_helpers.h"
#include "core/mutex.h"
#include <optional>
#include <unordered_map>
#include <functional>

namespace R3
{
	struct PooledBuffer
	{
		AllocatedBuffer m_buffer;
		uint64_t sizeBytes = 0;
		VkBufferUsageFlags m_usage = 0;
		VmaMemoryUsage m_memUsage = VMA_MEMORY_USAGE_UNKNOWN;
		void* m_mappedBuffer = 0;
		VkDeviceAddress m_deviceAddress;
		std::string m_name;
	};

	// Works as an allocator of VkBuffer
	// Freed buffers are kept around until a max footprint is hit (mem budget)
	//	free takes a few frames to ensure the released buffers are not being used by the gpu
	class BufferPool
	{
	public:
		BufferPool(std::string_view debugName, uint64_t totalBudget = 1024 * 1024 * 128);
		~BufferPool();

		std::optional<PooledBuffer> GetBuffer(std::string_view name, uint64_t minSizeBytes,VkBufferUsageFlags usage,VmaMemoryUsage memUsage, bool allowMapping);
		void Release(const PooledBuffer& buf);

		uint64_t GetTotalAllocatedBytes() { return m_totalAllocatedBytes; }
		uint64_t GetTotalAllocatedCount() { return m_totalAllocated; }
		uint64_t GetTotalCachedBytes() { return m_totalCachedBytes; }
		uint64_t GetTotalCachedCount() { return m_totalCached; }

		using CollectBufferStatFn = std::function<void(const PooledBuffer&)>;
		void CollectAllocatedBufferStats(CollectBufferStatFn fn);
		void CollectCachedBufferStats(CollectBufferStatFn fn);

	private:
		void CollectBufferStats(const std::unordered_map<uint64_t, PooledBuffer>& statsMap, CollectBufferStatFn fn);
		void CollectGarbage(uint64_t frameIndex, class Device& d);
		Mutex m_releasedBuffersMutex;
		struct ReleasedBuffer {
			PooledBuffer m_pooledBuffer;
			uint64_t m_frameReleased = -1;
		};
		std::vector<ReleasedBuffer> m_releasedBuffers;
		const uint64_t c_framesBeforeAvailable = 2;
		uint64_t m_totalBudget = 0;
		std::string m_debugName;

		// stat tracking, map key = buffer device address
		Mutex m_debugBuffersMutex;
		uint64_t m_totalAllocated = 0;
		uint64_t m_totalAllocatedBytes = 0;
		uint64_t m_totalCached = 0;
		uint64_t m_totalCachedBytes = 0;
		std::unordered_map<uint64_t, PooledBuffer> m_allocatedBuffers;	// track allocated + cached buffers for stats
		std::unordered_map<uint64_t, PooledBuffer> m_cachedBuffers;	// track allocated + cached buffers for stats
	};
}