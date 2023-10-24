#pragma once

#include <string_view>

namespace R3
{
	// filter format is as follows
	// a filter can contain multiple file formats separated by commas
	// semicolon separates each filter
	// 'bmp, jpg;fbx;mp4,wmv'

	std::string FileSaveDialog(std::string_view initialPath, std::string_view filter);	// returns a valid absolute path or an empty string
	std::string FileLoadDialog(std::string_view initialPath, std::string_view filter);	// returns a valid absolute path or an empty string
}