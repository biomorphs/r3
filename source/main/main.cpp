#include <string>
#include <fmt/format.h>
#include <cassert>
#include "core/profiler.h"
#include "core/platform.h"
#include "core/random.h"
#include "core/thread_pool.h"
#include "engine/thread_masks.h"

int main(int argc, char** args)
{
	auto result = R3::Platform::Initialise(argc, args);
	assert(result == R3::Platform::InitOK);
	if (result == R3::Platform::InitFailed)
	{
		return 1;
	}

	R3::Random::ResetGlobalSeed();

	R3::ThreadPool tp;
	tp.CreateThreads(R3::ThreadMasks::JobsSlow, "Jobs Slow", 3);
	tp.CreateThreads(R3::ThreadMasks::JobsFast, "Jobs Fast", 4);
	tp.Start();

	for (int i = 0; i < 500; ++i)
	{
		R3_PROF_FRAME("Main Thread");
		::_sleep(30);	
		tp.RunTasksImmediate(R3::ThreadMasks::Main);
		for (int j = 0; j < 10; ++j)
		{
			tp.PushTask([]() {
				R3_PROF_EVENT("Job");
				fmt::print("Hello there!");
				_sleep(20);
			}, R3::ThreadMasks::JobsFast);
		}
	}

	tp.Stop();

	// Shutdown
	auto shutdownResult = R3::Platform::Shutdown();
	assert(shutdownResult == R3::Platform::ShutdownOK);
	return shutdownResult == R3::Platform::ShutdownOK ? 0 : 1;
}
