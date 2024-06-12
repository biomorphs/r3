#pragma once
#include <unordered_map>
#include <vector>
#include <thread>
#include <optional>

struct VkCommandPool_T;
struct VkCommandBuffer_T;
namespace R3
{
	// we need pools per thread to enable multi-thread command buffers
	// this allocator keeps one pool per thread, and a free-list of buffers per pool
	// intended usage - allocate a buffer, fill it, draw with it, whatever, then release it back to the pool
	// after a few frames that buffer can be reused for that particular thread
	// over time, will store threads * frames * buffers used in pools, may need cleanup
	class Device;

	struct ManagedCommandBuffer
	{
		VkCommandBuffer_T* m_cmdBuffer = nullptr;
		std::thread::id m_ownerThread;
		bool m_isPrimary = false;
	};

	class CommandBufferAllocator
	{
	public:
		CommandBufferAllocator();
		~CommandBufferAllocator();
		VkCommandPool_T* GetPool(Device& d);	// gets/creates the pool for this thread
		std::optional<ManagedCommandBuffer> CreateCommandBuffer(Device& d, bool isPrimary);	// secondary = can't be directly submitted, can be called from other primary buffers
		void Release(ManagedCommandBuffer cmdBuffer);
		void Reset(Device& d);	// destroys all created command buffers!
		void Destroy(Device& d);
	private:
		struct ReleasedBuffer {
			VkCommandBuffer_T* m_buffer;
			bool m_isPrimary;
			uint64_t m_frameReleased;
		};
		struct PerThreadData {
			VkCommandPool_T* m_pool = nullptr;
			std::vector<ReleasedBuffer> m_releasedBuffers;
		};
		std::unordered_map<std::thread::id, PerThreadData> m_perThreadData;
		const uint64_t c_framesBeforeReuse = 4;	// how many frames before a released buffer can be re-allocated
	};
}