#pragma once

#include <string_view>

namespace R3
{
	namespace Platform
	{
		enum InitResult
		{
			InitOK,
			InitFailed,
		};

		enum ShutdownResult
		{
			ShutdownOK
		};

		struct SystemPowerState
		{
			int m_batterySecondsLeft = 0;	// approx how many seconds of battery life remain
			int m_batteryPercentageRemaining = 0;
			bool m_isRunningOnBattery = false;
		};
		SystemPowerState GetSystemPowerState();
		std::string_view GetCmdLine();
		InitResult Initialise(std::string_view fullCmdLine);
		ShutdownResult Shutdown();
	}
}