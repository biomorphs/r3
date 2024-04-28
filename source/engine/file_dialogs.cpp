#include "file_dialogs.h"
#include "core/file_io.h"
#include "core/log.h"
#include "core/profiler.h"
#include <nfd.h>
#include <filesystem>

namespace R3
{
    std::string FileLoadDialog(std::string_view initialPath, std::string_view filter)
    {
        R3_PROF_EVENT();
        std::string realInitialPath = initialPath.empty() ? std::string(FileIO::GetBasePath()) : FileIO::FindAbsolutePath(initialPath);
        nfdchar_t* newPath = NULL;
        nfdresult_t result = NFD_OpenDialog(filter.data(), realInitialPath.data(), &newPath);
        if (result == NFD_OKAY)
        {
            return newPath;
        }
        else if (result == NFD_ERROR)
        {
            LogError("Error when selecting file - {}", NFD_GetError());
        }
        return std::string();
    }

	std::string FileSaveDialog(std::string_view initialPath, std::string_view filter)
	{
        R3_PROF_EVENT();
        std::string realInitialPath = initialPath.empty() ? std::string(FileIO::GetBasePath()) : FileIO::FindAbsolutePath(initialPath);
        nfdchar_t* savePath = NULL;
        nfdresult_t result = NFD_SaveDialog(filter.data(), realInitialPath.data(), &savePath);
        if (result == NFD_OKAY)
        {
            return savePath;
        }
        else if (result == NFD_ERROR)
        {
            LogError("Error when selecting file - {}", NFD_GetError());
        }
		return std::string();
	}
}