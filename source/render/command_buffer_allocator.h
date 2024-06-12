#pragma once
#include <unordered_map>
#include <thread>

struct VkCommandPool_T;
struct VkCommandBuffer_T;
namespace R3
{
	// we need pools per thread to enable multi-thread command buffers
	class Device;
	class CommandBufferAllocator
	{
	public:
		CommandBufferAllocator();
		~CommandBufferAllocator();
		VkCommandPool_T* GetPool(Device& d);	// gets/creates the pool for this thread
		VkCommandBuffer_T* CreateCommandBuffer(Device& d, bool isPrimary);	// secondary = can't be directly submitted, can be called from other primary buffers
		void Reset(Device& d);	// destroys all created command buffers!
		void Destroy(Device& d);
	private:
		struct PerThreadData {
			VkCommandPool_T* m_pool = nullptr;
		};
		std::unordered_map<std::thread::id, PerThreadData> m_perThreadData;
	};
}