#pragma once
#include <string_view>
#include <optional>

namespace R3
{
	// returns the result code of the process, if it ran ok
	std::optional<uint32_t> RunProcess(std::string_view exeName, std::string_view cmdLine, bool waitForCompletion = true);
}