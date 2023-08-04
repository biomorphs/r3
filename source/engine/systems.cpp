#include "systems.h"
#include "core/profiler.h"

namespace R3
{
	Systems& Systems::GetInstance()
	{
		static Systems s_instance;
		return s_instance;
	}

	void Systems::RegisterTick(std::string_view name, std::function<bool()> fn)
	{
		TickFnRecord newRecord;
		newRecord.m_name = name;
		newRecord.m_fn = fn;
		m_allTicks.push_back(newRecord);
	}

	bool Systems::Initialise()
	{
		R3_PROF_EVENT();

		// Initialise first
		for (auto i = 0; i < m_allSystems.size(); ++i)
		{
			if (!m_allSystems[i].m_ptr->Init())
			{
				return false;
			}
		}

		// On success, register tick fns
		for (auto i = 0; i < m_allSystems.size(); ++i)
		{
			m_allSystems[i].m_ptr->RegisterTickFns();
		}

		return true;
	}

	void Systems::Shutdown()
	{
		R3_PROF_EVENT();

		// shutdown in reverse order of init
		for (int i = m_allSystems.size() - 1; i >= 0; --i)
		{
			m_allSystems[i].m_ptr->Shutdown();
		}

		m_allSystems.clear();
	}
}