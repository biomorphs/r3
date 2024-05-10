#include "job_pool.h"
#include "core/profiler.h"

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

	void JobPool::JobPoolThread(std::stop_token stoken)
	{
		R3_PROF_THREAD(GetName().data());
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

	JobPool::JobPool(int threadCount, std::string_view name)
		: m_name(name)
	{
		R3_PROF_EVENT();

		// start the threads 
		m_threads.reserve(threadCount);
		for (int i = 0; i < threadCount; ++i)
		{
			m_threads.emplace_back([this](std::stop_token stoken) {
				JobPoolThread(stoken);
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