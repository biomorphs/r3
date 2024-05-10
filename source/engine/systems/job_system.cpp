#include "job_system.h"
#include "core/profiler.h"
#include "core/job_pool.h"
#include <imgui.h>

namespace R3
{
	constexpr const char* c_jobPoolNames[] = {
		"Fast Jobs",
		"Slow Jobs"
	};

	void JobSystem::RegisterTickFns()
	{
		RegisterTick("Jobs::ShowGui", [this]() {
			return ShowGui();
		});
	}

	JobSystem::JobSystem()
	{
		R3_PROF_EVENT();
		m_jobPools.emplace_back(std::make_unique<JobPool>(6, JobPool::Priority::TimeCritical, "Fast Jobs"));	// Fast jobs
		m_jobPools.emplace_back(std::make_unique<JobPool>(4, JobPool::Priority::Normal, "Slow Jobs"));			// Slow jobs
	}

	JobSystem::~JobSystem()
	{
	}

	void JobSystem::Shutdown()
	{
		R3_PROF_EVENT();
		for (int i = 0; i < m_jobPools.size(); ++i)
		{
			m_jobPools[i]->StopAndWait();
		}
		m_jobPools.clear();
	}

	void JobSystem::PushJob(ThreadPool poolType, JobFn&& fn)
	{
		m_jobPools[poolType]->PushJob(std::move(fn));
	}

	void JobSystem::ProcessJobImmediate()
	{
		for (int i = 0; i < m_jobPools.size(); ++i)
		{
			if (m_jobPools[i]->RunJobImmediate())
			{
				return;
			}
		}
	}

	void JobSystem::ForEachAsync(ThreadPool poolType, int start, int end, int step, int stepsPerJob, ForEachJobFn fn)
	{
		R3_PROF_EVENT();
		std::atomic<int> jobsRemaining = 0;
		for (int32_t i = start; i < end; i += stepsPerJob)
		{
			const uint32_t startIndex = i;
			const uint32_t endIndex = std::min(i + stepsPerJob, end);
			auto runJob = [startIndex, endIndex, &jobsRemaining, &fn](void) {
				R3_PROF_EVENT("ForEachAsync");
				for (uint32_t c = startIndex; c < endIndex; ++c)
				{
					fn(c);
				}
				jobsRemaining--;
			};
			jobsRemaining++;
			PushJob(poolType, runJob);
		}

		// wait for the results
		{
			R3_PROF_STALL("WaitForResults");
			while (jobsRemaining > 0)
			{
				ProcessJobImmediate();
			}
		}
	}

	bool JobSystem::ShowGui()
	{
		R3_PROF_EVENT();
		if (m_showGui)
		{
			ImGui::Begin("Jobs");
			for (int i = 0; i < m_jobPools.size(); ++i)
			{
				std::string txt = std::format("Pending {}: {}", c_jobPoolNames[i], m_jobPools[i]->JobsPending());
				ImGui::Text(txt.c_str());
			}
			ImGui::End();
		}
		return true;
	}
}