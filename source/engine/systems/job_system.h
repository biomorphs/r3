#pragma once
#include "engine/systems.h"
#include <vector>

namespace R3
{
	class JobPool;
	class JobSystem : public R3::System
	{
	public:
		JobSystem();
		virtual ~JobSystem();
		static std::string_view GetName() { return "Jobs"; }
		virtual void RegisterTickFns();
		virtual void Shutdown();

		enum ThreadPool {
			FastJobs,	// jobs that are expected to finish asap
			SlowJobs	// jobs that we can afford to wait on
		};
		using JobFn = std::function<void()>;
		void PushJob(ThreadPool poolType, JobFn&&);

	private:
		bool ShowGui();
		std::vector<std::unique_ptr<JobPool>> m_jobPools;
	};
}