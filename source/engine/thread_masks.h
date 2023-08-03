#pragma once

namespace R3
{
	enum ThreadMasks : uint64_t
	{
		All = 0xffffffff,
		Main = (1 << 0),
		JobsFast = (1 << 1),
		JobsSlow = (1 << 2),
	};
}