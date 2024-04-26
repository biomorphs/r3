#pragma once
#include <vector>
#include <thread>
#include <functional>
#include <concurrentqueue/blockingconcurrentqueue.h>

struct SDL_Thread;
namespace R3
{
	// Job pool represents a number of threads with a shared queue of jobs to run
	class JobPool
	{
	public:
		explicit JobPool(int threadCount);
		~JobPool();
		using JobFn = std::function<void()>;
		void PushJob(JobFn&& fn);
		int JobsPending();
		void WaitUntilComplete();
	private:
		std::vector<std::jthread> m_threads;
		moodycamel::BlockingConcurrentQueue<JobFn> m_jobs;
		std::atomic<int> m_jobsPending = 0;
	};
}