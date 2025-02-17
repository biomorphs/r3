#pragma once 

#include "vulkan_helpers.h"
#include "buffer_pool.h"

namespace R3
{
	class Device;

	// a buffer that only supports linear writes from a single thread
	// mainly used for device-local memory with write-combined staging buffer
	// MUCH faster than WriteOnlyGpuBuffer but less flexible
	// no random writes, flush + append only
	// no thread safety
	// fastest write API is GetWritePtr()..., direct write to staging buffer
	//	you MUST to do your own bounds checks!
	//  you CANNOT cache the write ptr beyond a call to flush
	//  you MUST call CommitWrites(size_t) to increment m_writeOffset + ensure staging buffer is copied correctly
	
	class LinearWriteGpuBuffer
	{
	public:
		LinearWriteGpuBuffer() = default;
		LinearWriteGpuBuffer(LinearWriteGpuBuffer&&) = delete;
		LinearWriteGpuBuffer(const LinearWriteGpuBuffer&) = delete;

		bool Create(std::string_view name, Device& d, uint32_t dataMaxSize, VkBufferUsageFlags usageFlags);
		uint32_t Write(uint32_t sizeBytes, const void* data);	// writes to staging buffer, returns offset where data was written
		uint8_t* GetWritePtr() const;							// returns ptr to staging buffer at current 'head'. fastest write path. You MUST commit the writes using CommitWrites
		void CommitWrites(uint32_t sizeBytes);					// increments m_writeOffset, basic bounds checking
		void RetirePooledBuffer(Device&);		// old buffer is retired + a new one is created, pooled buffers only. call this before flushing writes
		void Flush(Device& d, VkCommandBuffer cmds, VkPipelineStageFlags barrierDst = VK_PIPELINE_STAGE_VERTEX_INPUT_BIT);	// does one big copy from staging to target forall previous writes
		bool IsCreated();
		void Destroy(Device& d);

		VkDeviceAddress GetBufferDeviceAddress();
		VkBuffer GetBuffer();
		uint32_t GetMaxSize() const { return m_maxSize; }
		uint32_t GetSizeWritten() const { return m_writeOffset; }

	private:
		bool AcquireNewStagingBuffer(Device& d);
		bool AcqureNewTargetBuffer(Device& d);

		// next write offset
		uint32_t m_writeOffset = 0;

		// keep track of flags used during creation
		VkBufferUsageFlags m_usageFlags;
		uint32_t m_maxSize;
		std::string m_debugName;

		PooledBuffer m_stagingBuffer;
		PooledBuffer m_pooledBuffer;
	};

	// A linear-write array of gpu data
	template<class Type>
	class LinearWriteOnlyGpuArray
	{
	public:
		LinearWriteOnlyGpuArray() = default;
		LinearWriteOnlyGpuArray(const LinearWriteOnlyGpuArray&) = delete;
		LinearWriteOnlyGpuArray(LinearWriteOnlyGpuArray&&) = delete;
		bool Create(std::string_view name, Device& d, uint32_t maxCount, VkBufferUsageFlags usageFlags)
		{
			return m_buffer.Create(name, d, maxCount * sizeof(Type), usageFlags);
		}
		uint32_t Write(uint32_t count, const Type* data)
		{
			return m_buffer.Write(count * sizeof(Type), data) / sizeof(Type);
		}
		Type* GetWritePtr() const
		{
			return reinterpret_cast<Type*>(m_buffer.GetWritePtr());
		}
		void CommitWrites(uint32_t count)
		{
			m_buffer.CommitWrites(count * sizeof(Type));
		}
		void RetirePooledBuffer(Device& d)
		{
			m_buffer.RetirePooledBuffer(d);
		}
		void Flush(Device& d, VkCommandBuffer cmds, VkPipelineStageFlags barrierDst = VK_PIPELINE_STAGE_VERTEX_INPUT_BIT)
		{
			m_buffer.Flush(d, cmds, barrierDst);
		}
		bool IsCreated()
		{
			return m_buffer.IsCreated();
		}
		void Destroy(Device& d)
		{
			m_buffer.Destroy(d);
		}
		VkDeviceAddress GetBufferDeviceAddress()
		{
			return m_buffer.GetBufferDeviceAddress();
		}
		VkBuffer GetBuffer()
		{
			return m_buffer.GetBuffer();
		}
	private:
		LinearWriteGpuBuffer m_buffer;
	};
}