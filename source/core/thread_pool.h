#pragma once
#include <string>
#include <stdint.h>
#include <functional>
#include <atomic>
#include <memory>
#include "core/semaphore.h"

namespace R3
{
	// owns a list of threads, each given a mask
	// tasks can be submitted, they will run on a thread with matching mask
	class ThreadPool
	{
	public:
		ThreadPool();
		ThreadPool(ThreadPool&&) = delete;
		ThreadPool(const ThreadPool&) = delete;
		~ThreadPool();

		void CreateThreads(uint64_t threadMask, const char* name, uint32_t count);
		void Start();
		void Stop();

		using TaskFn = std::function<void()>;
		void PushTask(const TaskFn& fn, uint64_t threadMask=0xffffffffffffffff);

		bool RunTasksImmediate(uint64_t threadMask);

	private:
		bool GetTaskToRun(uint64_t threadMask, TaskFn& fnToRun);

		class TaskList;
		struct ThreadDescriptor
		{
			ThreadPool* m_parent;
			std::string m_name;
			void* m_handle;
			uint64_t m_mask;
			uint64_t m_index;
		};
		static int32_t RunThread(void* data);
		std::unique_ptr<TaskList> m_tasks;
		std::vector<ThreadDescriptor> m_threads;
		std::atomic<int32_t> m_stopRequested;
		Semaphore m_jobAddedTrigger;
	};
}