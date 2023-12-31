#include "time_system.h"
#include "core/time.h"
#include "core/profiler.h"

namespace R3
{
	void TimeSystem::RegisterTickFns()
	{
		R3_PROF_EVENT();
		Systems::GetInstance().RegisterTick("Time::FrameStart", [this]() {
			return OnFrameStart();
		});
		Systems::GetInstance().RegisterTick("Time::FixedUpdateEnd", [this]() {
			return OnFixedUpdateEnd();
		});
	}

	double TimeSystem::GetElapsedTimeReal() const
	{
		return (R3::Time::HighPerformanceCounterTicks() - m_firstTickTime) / double(m_tickFrequency);
	}

	double TimeSystem::GetElapsedTime() const
	{
		return (m_thisFrameTickTime - m_firstTickTime) / double(m_tickFrequency);
	}

	double TimeSystem::GetVariableDeltaTime() const
	{
		return (m_thisFrameTickTime - m_lastTickTime) / double(m_tickFrequency);
	}

	double TimeSystem::GetFixedUpdateCatchupTime() const
	{
		return m_fixedUpdateCatchup / double(m_tickFrequency);
	}

	double TimeSystem::GetFixedUpdateDelta() const
	{
		return m_fixedUpdateDeltaTime / double(m_tickFrequency);
	}

	bool TimeSystem::OnFrameStart()
	{
		R3_PROF_EVENT();
		if (m_firstTick)
		{
			m_firstTickTime = R3::Time::HighPerformanceCounterTicks();
			m_thisFrameTickTime = m_firstTickTime;
			m_lastTickTime = m_thisFrameTickTime;
			m_firstTick = false;
		}

		m_lastTickTime = m_thisFrameTickTime;
		m_thisFrameTickTime = R3::Time::HighPerformanceCounterTicks();
		uint64_t elapsed = m_thisFrameTickTime - m_lastTickTime;
		
		m_fixedUpdateCatchup += elapsed;

		return true;
	}

	bool TimeSystem::OnFixedUpdateEnd()
	{
		R3_PROF_EVENT();
		m_fixedUpdateCatchup -= m_fixedUpdateDeltaTime;
		return true;
	}

	bool TimeSystem::Init()
	{
		R3_PROF_EVENT();
		m_tickFrequency = R3::Time::HighPerformanceCounterFrequency();
		m_initTime = R3::Time::HighPerformanceCounterTicks();
		m_fixedUpdateDeltaTime = m_tickFrequency / m_fixedUpdateFPS;
		return true;
	}
}