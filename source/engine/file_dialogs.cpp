#include "file_dialogs.h"
#include "core/file_io.h"
#include "core/log.h"
#include "core/profiler.h"
#include <nfd.h>
#include <filesystem>

namespace R3
{
    std::vector<nfdu8filteritem_t> GetNFDFilters(const FileDialogFilter* filters, size_t filterCount)
    {
        std::vector<nfdu8filteritem_t> nfdFilters;
        for (int f = 0; f < filterCount; ++f)
        {
            nfdu8filteritem_t filter;
            filter.name = filters[f].m_name.c_str();
            filter.spec = filters[f].m_extensions.c_str();
            nfdFilters.push_back(filter);
        }
        return nfdFilters;
    }
    
    std::string FileLoadDialog(std::string_view initialPath, const FileDialogFilter* filters, size_t filterCount)
    {
        R3_PROF_EVENT();
        std::string realInitialPath = initialPath.empty() ? std::string(FileIO::GetBasePath()) : FileIO::FindAbsolutePath(initialPath);
        nfdchar_t* newPath = NULL;
        auto nfdFilters = GetNFDFilters(filters, filterCount);
        nfdresult_t result = NFD_OpenDialog(&newPath, nfdFilters.data(), (nfdfiltersize_t)nfdFilters.size(), initialPath.data());
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

	std::string FileSaveDialog(std::string_view initialPath, const FileDialogFilter* filters, size_t filterCount)
	{
        R3_PROF_EVENT();
        std::string realInitialPath = initialPath.empty() ? std::string(FileIO::GetBasePath()) : FileIO::FindAbsolutePath(initialPath);
        nfdchar_t* savePath = NULL;
        auto nfdFilters = GetNFDFilters(filters, filterCount);
        nfdresult_t result = NFD_SaveDialog(&savePath, nfdFilters.data(), (nfdfiltersize_t)nfdFilters.size(), realInitialPath.data(), initialPath.data());
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