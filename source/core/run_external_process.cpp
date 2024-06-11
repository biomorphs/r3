#include "run_external_process.h"
#include "log.h"
#include "file_io.h"
#include "profiler.h"
#ifdef _WIN32
#include <windows.h>
#endif
#include <cassert>

namespace R3
{
    std::wstring stringToWide(std::string_view s)
    {
        static_assert(sizeof(std::wstring::value_type) == sizeof(wchar_t));
        std::wstring newBuffer;
        if (s.empty())
            return {};
        size_t charsNeeded = ::MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), NULL, 0);
        if (charsNeeded == 0)
            return {};
        newBuffer.resize(charsNeeded);
        int charsConverted = ::MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), newBuffer.data(), (int)charsNeeded);
        if (charsConverted == 0)
        {
            LogWarn("Failed to convert string '{}' to wide", s);
            return {};
        }
        return newBuffer;
    }

    std::optional<uint32_t> RunProcess(std::string_view exeName, std::string_view cmdLine, bool waitForCompletion)
	{
        R3_PROF_EVENT();
        
#ifdef _WIN32
		// Always append exe name to full cmd line
        // (its recommended to always surround the cmd line in "" to avoid attack strings)
		std::string fullCmdLine = std::string(exeName) + " " + std::string(cmdLine);
        // win32 uses wide strings for paths
        auto fullCmdLineWide = stringToWide(fullCmdLine);
        auto workingDir = stringToWide(FileIO::GetBasePath());
        STARTUPINFOW si = { 0 };
        si.cb = sizeof(si);
        PROCESS_INFORMATION pi = { 0 };
        if(CreateProcessW(nullptr, fullCmdLineWide.data(),
            nullptr, nullptr, false, 0, nullptr, workingDir.data(), &si, &pi) == 0)
        {
            LogError("CreateProcess failed {}", GetLastError());
            return {};
        }
        DWORD exitCode = 0;
        if (waitForCompletion)
        {
            WaitForSingleObject(pi.hProcess, INFINITE);
            GetExitCodeProcess(pi.hProcess, &exitCode);
        }
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
#endif
        return exitCode;
	}
}