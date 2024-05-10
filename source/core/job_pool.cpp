#include "job_pool.h"
#include "core/profiler.h"
#include "core/log.h"
#include <SDL_thread.h>

namespace R3
{
	void JobPool::StopAndWait()
	{
		for (auto& thread : m_threads)
		{
			thread.request_stop();
		}
		for (auto& thread : m_threads)
		{
			thread.join();
		}
		m_threads.clear();
	}

	void JobPool::JobPoolThread(std::stop_token stoken, Priority p)
	{
		R3_PROF_THREAD(GetName().data());
		SDL_ThreadPriority prio = SDL_ThreadPriority::SDL_THREAD_PRIORITY_NORMAL;
		switch (p)
		{
		case Priority::Low:
			prio = SDL_ThreadPriority::SDL_THREAD_PRIORITY_LOW;
			break;
		case Priority::Normal:
			prio = SDL_ThreadPriority::SDL_THREAD_PRIORITY_NORMAL;
			break;
		case Priority::High:
			prio = SDL_ThreadPriority::SDL_THREAD_PRIORITY_HIGH;
			break;
		case Priority::TimeCritical:
			prio = SDL_ThreadPriority::SDL_THREAD_PRIORITY_TIME_CRITICAL;
			break;
		}
		if (SDL_SetThreadPriority(prio) != 0)
		{
			LogWarn("Failed to set priority for thread - {}", SDL_GetError());
		}

		while (!stoken.stop_requested())
		{
			JobFn task;
			bool taskDequeud = false;
			{
				R3_PROF_STALL("WaitForJobs");
				taskDequeud = m_jobs.wait_dequeue_timed(task, 1000);	// this will return false if timeout hits
			}
			if (taskDequeud)
			{
				task();
				m_jobsPending.fetch_add(-1, std::memory_order_release);
			}
		}
	}

	JobPool::JobPool(int threadCount, Priority p, std::string_view name)
		: m_name(name)
	{
		R3_PROF_EVENT();

		// start the threads 
		m_threads.reserve(threadCount);
		for (int i = 0; i < threadCount; ++i)
		{
			m_threads.emplace_back([this, p](std::stop_token stoken) {
				JobPoolThread(stoken, p);
			});
		}
	}

	JobPool::~JobPool()
	{
		StopAndWait();
	}

	void JobPool::PushJob(JobFn&& fn)
	{
		m_jobsPending.fetch_add(1, std::memory_order_release);
		m_jobs.enqueue(std::move(fn));
	}

	int JobPool::JobsPending()
	{
		return m_jobsPending.load();
	}

	bool JobPool::RunJobImmediate()
	{
		JobFn task;
		if (m_jobs.try_dequeue(task)) 
		{
			task();
			m_jobsPending.fetch_add(-1, std::memory_order_release);
			return true;
		}

		return false;
	}


	void JobPool::WaitUntilComplete()
	{
		R3_PROF_STALL("WaitUntilComplete");
		// wait for all the jobs to complete
		// any pending jobs will be picked up by the current thread if possible
		JobFn task;
		while (m_jobsPending.load(std::memory_order_acquire) != 0) 
		{
			if (!m_jobs.try_dequeue(task)) 
			{
				continue;
			}
			task();
			m_jobsPending.fetch_add(-1, std::memory_order_release);
		}
	}
}