#pragma once

#include <stdint.h>

namespace R3
{
	namespace Time
	{
		uint64_t HighPerformanceCounterTicks();
		uint64_t HighPerformanceCounterFrequency();
	}
}