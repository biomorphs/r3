#include "async.h"
#include "core/profiler.h"
#include "core/file_io.h"
#include "core/log.h"

namespace R3
{
    void RunAsync(JobFn&& fn, JobSystem::ThreadPool t)
    {
        Systems::GetSystem<JobSystem>()->PushJob(t, std::move(fn));
    }

    void RunAsyncThen(JobFn&& fn, JobFn&& then, JobSystem::ThreadPool t)
    {
        auto runThen = [fn, then]()
        {
            R3_PROF_EVENT("RunAsyncThen");
            fn();
            then();
        };
        RunAsync(std::move(runThen), t);
    }

    void LoadBinaryFileThen(std::string_view filePath, LoadBinaryFileCompletion&& onCompletion)
    {
        std::string pathStr(filePath);
        auto loadFileThen = [pathStr, onCompletion]() {
            char debugName[1024] = { '\0' };
            sprintf_s(debugName, "LoadBinary %s", pathStr.c_str());
            R3_PROF_EVENT_DYN(debugName);
            std::vector<uint8_t> rawData;
            if (!FileIO::LoadBinaryFile(pathStr, rawData))
            {
                LogWarn("Failed to load binary file {}", pathStr);
                onCompletion(pathStr, false, rawData);
            }
            else
            {
                onCompletion(pathStr, true, rawData);
            }
        };
        RunAsync(loadFileThen, R3::JobSystem::SlowJobs);
    }
}