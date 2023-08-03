#include "thread_pool.h"
#include "core/profiler.h"
#include "core/mutex.h"
#include <deque>
#include <SDL_thread.h>

namespace R3
{
	class ThreadPool::TaskList
	{
	public:
		TaskList()
		{
		}
		void Push(uint64_t threadMask, TaskFn fn)
		{
			ScopedLock lock(m_lock);
			m_tasks.push_front({threadMask, fn});
		}
		bool Pop(uint64_t threadMask, TaskFn& fn, bool& hasAnyTasks)
		{
			ScopedLock lock(m_lock);
			hasAnyTasks = m_tasks.size() > 0;
			for (int i = m_tasks.size() - 1; i >= 0; --i)
			{
				if (m_tasks[i].threadMask == 0 || (m_tasks[i].threadMask & threadMask) == m_tasks[i].threadMask)
				{
					fn = std::move(m_tasks[i].m_fn);
					m_tasks.erase(m_tasks.begin() + i);
					return true;
				}
			}
			return false;
		}
	private:
		Mutex m_lock;
		struct TaskDesc {
			uint64_t threadMask;
			TaskFn m_fn;
		};
		std::deque<TaskDesc> m_tasks;
	};

	ThreadPool::ThreadPool()
		: m_stopRequested(0)
		, m_jobAddedTrigger(0)
	{
		m_threads.reserve(128);
		m_tasks = std::make_unique<ThreadPool::TaskList>();
	}

	ThreadPool::~ThreadPool()
	{

	}

	bool ThreadPool::RunTasksImmediate(uint64_t threadMask)
	{
		R3_PROF_EVENT();

		TaskFn toRun = nullptr;
		if (GetTaskToRun(threadMask, toRun))
		{
			toRun();
			return true;
		}
		return false;
	}

	void ThreadPool::PushTask(const TaskFn& fn, uint64_t threadMask)
	{
		m_tasks->Push(threadMask, fn);
		m_jobAddedTrigger.Post();
	}

	bool ThreadPool::GetTaskToRun(uint64_t threadMask, TaskFn& fnToRun)
	{
		bool hasAnyTasks = false;
		bool shouldRun = m_tasks->Pop(threadMask, fnToRun, hasAnyTasks);
		if (!shouldRun && hasAnyTasks)	// if there are tasks but we can't run them, retrigger other threads
		{
			m_jobAddedTrigger.Post();
		}
		return shouldRun;
	}

	int32_t ThreadPool::RunThread(void* data)
	{
		const ThreadDescriptor* td = reinterpret_cast<ThreadPool::ThreadDescriptor*>(data);
		R3_PROF_THREAD(td->m_name.c_str());

		TaskFn nextTaskFn = nullptr;
		while (!td->m_parent->m_stopRequested)
		{
			{
				R3_PROF_STALL("WaitForJobsTrigger");
				td->m_parent->m_jobAddedTrigger.Wait();		// Wait for jobs
			}
			if (td->m_parent->m_stopRequested == 0)
			{
				if (td->m_parent->GetTaskToRun(td->m_mask, nextTaskFn))
				{
					nextTaskFn();
				}
			}
		}

		return 0;
	}

	void ThreadPool::CreateThreads(uint64_t threadMask, const char* name, uint32_t count)
	{
		R3_PROF_EVENT();
		uint64_t newThreadIndex = m_threads.size();
		for (uint32_t i = 0; i < count; ++i)
		{
			m_threads.emplace_back(ThreadDescriptor{this, name, nullptr, threadMask, newThreadIndex++});
			newThreadIndex++;
		}
	}

	void ThreadPool::Start()
	{
		R3_PROF_EVENT();
		for (auto& td : m_threads)
		{
			td.m_handle = SDL_CreateThread(RunThread, td.m_name.c_str(), reinterpret_cast<void*>(&td));
		}
	}
	
	void ThreadPool::Stop()
	{
		R3_PROF_EVENT();
		m_stopRequested = true;
		for (auto& it : m_threads)
		{
			int32_t result = 0;
			R3_PROF_STALL("Wait for thread stop");
			m_jobAddedTrigger.Post();	// ensure they all wake up
			SDL_WaitThread(static_cast<SDL_Thread*>(it.m_handle), &result);
		}
		m_threads.clear();
	}
}