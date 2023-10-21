#pragma once

#include <string_view>
#include <string>
#include <vector>

// Helpers for loading raw data from external files
namespace R3
{
	namespace FileIO
	{
		// paths can be relative to base path(s) or absolute
		bool LoadTextFromFile(std::string_view filePath, std::string& resultBuffer);
		bool SaveTextToFile(std::string_view filePath, const std::string& src);
		bool LoadBinaryFile(std::string_view filePath, std::vector<uint8_t>& resultBuffer);
		bool SaveBinaryFile(std::string_view filePath, const std::vector<uint8_t>& src);

		// system path stuff
		void InitialisePaths();
		void AddBasePath(std::string_view path);	// add a folder to search when loading files
		const std::string_view GetBasePath();

		// queries/enumeration
		std::string FindAbsolutePath(std::string_view filePath);	// find a file, return its absolute path (or empty string if not found)
	}
}