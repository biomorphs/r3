#pragma once

#include <string_view>

namespace R3
{
	// filter format is as follows
	// a filter can contain multiple file formats separated by commas
	// semicolon separates each filter
	// 'bmp, jpg;fbx;mp4,wmv

	struct FileDialogFilter
	{
		std::string m_name;				// e.g. "images"
		std::string m_extensions;		// e.g. "jpg,bmp,png,tga"
	};

	std::string FileSaveDialog(std::string_view initialPath, const FileDialogFilter* filters, size_t filterCount);	// returns a valid absolute path or an empty string
	std::string FileLoadDialog(std::string_view initialPath, const FileDialogFilter* filters, size_t filterCount);	// returns a valid absolute path or an empty string
}