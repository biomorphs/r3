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

		std::string_view GetCmdLine();
		InitResult Initialise(std::string_view fullCmdLine);
		ShutdownResult Shutdown();
	}
}