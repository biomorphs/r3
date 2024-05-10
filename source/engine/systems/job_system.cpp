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
		m_jobPools.emplace_back(std::make_unique<JobPool>(4, "Fast Jobs"));	// Fast jobs
		m_jobPools.emplace_back(std::make_unique<JobPool>(4, "Slow Jobs"));	// Slow jobs
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