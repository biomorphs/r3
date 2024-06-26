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
		enum class Priority {
			Low,
			Normal,
			High,
			TimeCritical
		};
		explicit JobPool(int threadCount, Priority p, std::string_view name);
		~JobPool();
		using JobFn = std::function<void()>;
		void PushJob(JobFn&& fn);
		int JobsPending();
		bool RunJobImmediate();	// try to run a job now on this thread, return true if it ran anything
		void StopAndWait();	// does not wait for pending jobs to finish
		std::string_view GetName() { return m_name; }
	private:
		void WaitUntilComplete();
		void JobPoolThread(std::stop_token stoken, Priority p);
		std::vector<std::jthread> m_threads;
		moodycamel::BlockingConcurrentQueue<JobFn> m_jobs;
		std::atomic<int> m_jobsPending = 0;
		std::string m_name;
	};
}