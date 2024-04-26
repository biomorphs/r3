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
		explicit JobPool(int threadCount, std::string_view name);
		~JobPool();
		using JobFn = std::function<void()>;
		void PushJob(JobFn&& fn);
		int JobsPending();
		void StopAndWait();	// does not wait for pending jobs to finish
		std::string_view GetName() { return m_name; }
	private:
		void WaitUntilComplete();
		void JobPoolThread(std::stop_token stoken);
		std::vector<std::jthread> m_threads;
		moodycamel::BlockingConcurrentQueue<JobFn> m_jobs;
		std::atomic<int> m_jobsPending = 0;
		std::string m_name;
	};
}