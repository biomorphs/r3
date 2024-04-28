#include "file_io.h"
#include "profiler.h"
#include "log.h"
#include <cassert>
#include <fstream>
#include <filesystem>
#include <SDL_filesystem.h>

namespace R3
{
	namespace FileIO
	{
		std::vector<std::string> c_extraDirectories;
		std::string c_baseDirectory = "";
		std::string c_exeDirectory = "";

		std::string FindAbsolutePath(std::string_view filePath)
		{
			std::filesystem::path p(filePath);
			if (p.is_absolute())
			{
				if (std::filesystem::exists(p))
				{
					return p.string();
				}
			}
			else
			{
				for (int d = 0; d < c_extraDirectories.size(); d++)
				{
					std::string fullPath = c_extraDirectories[d] + "/";
					std::filesystem::path pp(fullPath + std::string(filePath));
					if (std::filesystem::exists(pp))
					{
						return std::filesystem::absolute(pp).string();
					}
				}
				if (std::filesystem::exists(c_baseDirectory + std::string(filePath)))
				{
					return std::filesystem::absolute(c_baseDirectory + std::string(filePath)).string();
				}
			}
			return "";
		}

		void InitialisePaths()
		{
			c_baseDirectory = std::filesystem::current_path().concat("\\").string();
			c_exeDirectory = std::string(SDL_GetBasePath()) + "\\";
		}

		void AddBasePath(std::string_view path)
		{
			std::string strPath(path);
			auto found = std::find(c_extraDirectories.begin(), c_extraDirectories.end(), strPath);
			if (found == c_extraDirectories.end())
			{
				c_extraDirectories.push_back(strPath);
			}
		}

		const std::string_view GetBasePath()
		{
			return c_baseDirectory;
		}

		bool SaveTextToFile(std::string_view filePath, const std::string& src)
		{
			R3_PROF_EVENT();
			if (!std::filesystem::path(filePath).is_absolute())
			{
				LogError("Only absolute paths are accepted when writing files ('{}' is not valid)", filePath);
				return false;
			}
			std::ofstream fileStream(filePath.data(), std::ios::out);
			if (!fileStream.is_open())
			{
				return false;
			}
			fileStream.write((const char*)src.data(), src.size());
			fileStream.close();
			return true;
		}

		bool LoadTextFromFile(std::string_view fileSrcPath, std::string& resultBuffer)
		{
			R3_PROF_EVENT();
			std::string actualPath = FindAbsolutePath(fileSrcPath);
			if (actualPath.empty())
			{
				LogWarn("File '{}' not found", fileSrcPath);
				return false;
			}
			resultBuffer.clear();
			std::ifstream fileStream(actualPath.data(), std::ios::in);
			if (!fileStream.is_open())
			{
				return false;
			}

			fileStream.seekg(0, fileStream.end);
			const size_t fileSize = fileStream.tellg();
			fileStream.seekg(0, fileStream.beg);

			resultBuffer.resize(fileSize);
			fileStream.read(resultBuffer.data(), fileSize);

			size_t actualSize = strlen(resultBuffer.data());
			resultBuffer.resize(actualSize);

			return true;
		}

		bool LoadBinaryFile(std::string_view fileSrcPath, std::vector<uint8_t>& resultBuffer)
		{
			R3_PROF_EVENT();
			std::string actualPath = FindAbsolutePath(fileSrcPath);
			if (actualPath.empty())
			{
				LogWarn("File '{}' not found", fileSrcPath);
				return false;
			}
			std::ifstream fileStream(actualPath.data(), std::ios::binary | std::ios::in);
			if (!fileStream.is_open())
			{
				return false;
			}

			fileStream.seekg(0, fileStream.end);
			const size_t fileSize = fileStream.tellg();
			fileStream.seekg(0, fileStream.beg);

			resultBuffer.resize(fileSize);
			fileStream.read(reinterpret_cast<char*>(resultBuffer.data()), fileSize);
			fileStream.close();

			return true;
		}

		bool SaveBinaryFile(std::string_view filePath, const std::vector<uint8_t>& src)
		{
			R3_PROF_EVENT();
			if (!std::filesystem::path(filePath).is_absolute())
			{
				LogError("Only absolute paths are accepted when writing files ('{}' is not valid)", filePath);
				return false;
			}
			std::ofstream fileStream(filePath.data(), std::ios::binary | std::ios::out);
			if (!fileStream.is_open())
			{
				return false;
			}
			fileStream.write((const char*)src.data(), src.size());
			fileStream.close();
			return true;
		}
	}
}