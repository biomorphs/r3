#include "job_pool.h"
#include "core/profiler.h"

namespace R3
{
	JobPool::JobPool(int threadCount)
	{
		R3_PROF_EVENT();

		// start the threads 
		m_threads.reserve(threadCount);
		for (int i = 0; i < threadCount; ++i)
		{
			std::jthread newThread([this]() {
				R3_PROF_THREAD("JobPool");
				while (true)
				{
					JobFn task;
					{
						R3_PROF_STALL("WaitForJobs");
						m_jobs.wait_dequeue(task);
					}
					task();
					m_jobsPending.fetch_add(-1, std::memory_order_release);
				}
			});
			newThread.detach();
		}
	}

	JobPool::~JobPool()
	{
		WaitUntilComplete();
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

	void JobPool::WaitUntilComplete()
	{
		R3_PROF_STALL("WaitUntilComplete");
		// wait for all the jobs to complete
		// any pending jobs will be picked up by the current thread if possible
		JobFn task;
		while (m_jobsPending.load(std::memory_order_acquire) != 0) 
		{
			if (!m_jobs.try_dequeue(task)) {
				continue;
			}
			task();
			m_jobsPending.fetch_add(-1, std::memory_order_release);
		}
	}
}