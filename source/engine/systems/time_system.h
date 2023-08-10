#pragma once
#include "engine/systems.h"

namespace R3
{
	class TimeSystem : public System
	{
	public:
		static std::string_view GetName() { return "Time"; }
		virtual void RegisterTickFns();
		virtual bool Init();

		double GetElapsedTimeReal() const;			// return the actual elapsed time
		double GetElapsedTime() const;				// total time from first tick to current variable tick
		double GetVariableDeltaTime() const;		// variable time delta from previous frame
		double GetFixedUpdateCatchupTime() const;	// how much time does fixed update need to catch up
		double GetFixedUpdateDelta() const;			// fixed update delta time

	private:
		bool OnFrameStart();		// call this early in frame
		bool OnFixedUpdateEnd();	// call this after each fixed update

		uint64_t m_tickFrequency = 0;
		uint64_t m_initTime = 0;
		uint64_t m_firstTickTime = 0;
		uint64_t m_lastTickTime = 0;
		uint64_t m_thisFrameTickTime = 0;
		uint64_t m_fixedUpdateCatchup = 0;
		uint64_t m_fixedUpdateDeltaTime = 0;
		uint64_t m_fixedUpdateCurrentTime = 0;
		static constexpr int m_fixedUpdateFPS = 60;
		static constexpr double m_updateFrequency = 1.0 / (double)m_fixedUpdateFPS;

		bool m_firstTick = true;
	};
}