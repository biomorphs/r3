#pragma once
#include "systems/job_system.h"

// Helpers + shorthand for common async tasks
namespace R3
{
	using JobFn = std::function<void()>;

	// Run a single job with no completion checks
	void RunAsync(JobFn&& fn, JobSystem::ThreadPool t = JobSystem::ThreadPool::FastJobs);

	// Run a single job, when it completes, do something else on the same thread
	void RunAsyncThen(JobFn&& fn, JobFn&& then, JobSystem::ThreadPool t = JobSystem::ThreadPool::FastJobs);

	// Load a file, then do something with the result

	// Params = file name loaded, errorsEncountered, data read from file
	using LoadBinaryFileCompletion = std::function<void(std::string_view, bool, const std::vector<uint8_t>&)>;
	void LoadBinaryFileThen(std::string_view filePath, LoadBinaryFileCompletion&& onCompletion);
}