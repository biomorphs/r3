#include "time_system.h"
#include "engine/systems/lua_system.h"
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

	double TimeSystem::GetFixedUpdateInterpolation() const
	{
		return m_fixedUpdateInterpolation;
	}

	uint64_t TimeSystem::GetFrameIndex() const
	{
		return m_frameIndex;
	}

	bool TimeSystem::OnFrameStart()
	{
		R3_PROF_EVENT();
		if (m_firstTick)
		{
			m_firstTickTime = R3::Time::HighPerformanceCounterTicks();
			m_thisFrameTickTime = m_firstTickTime;
			m_firstTick = false;
		}
		m_frameIndex++;
		m_lastTickTime = m_thisFrameTickTime;
		m_thisFrameTickTime = R3::Time::HighPerformanceCounterTicks();
		uint64_t elapsed = m_thisFrameTickTime - m_lastTickTime;
		
		m_fixedUpdateCatchup += elapsed;

		// Each frame, calculate the interpolation factor based on how far ahead we are from the fixed update
		const double catchUpSeconds = (double)m_fixedUpdateCatchup / (double)m_tickFrequency;
		const double fuDeltaSeconds = (double)m_fixedUpdateDeltaTime / (double)m_tickFrequency;
		m_fixedUpdateInterpolation = catchUpSeconds / fuDeltaSeconds;
		m_fixedUpdateInterpolation = m_fixedUpdateInterpolation - floor(m_fixedUpdateInterpolation);	// just keep fractional part
		LogInfo("cu {}, fui {}", catchUpSeconds, m_fixedUpdateInterpolation);

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

		auto scripts = Systems::GetSystem<LuaSystem>();
		if (scripts)
		{
			scripts->RegisterFunction("GetFixedUpdateDelta", [this]() -> double {
				return GetFixedUpdateDelta();
			});
			scripts->RegisterFunction("GetVariableDelta", [this]() -> double {
				return GetVariableDeltaTime();
			});
		}
		return true;

		return true;
	}
}