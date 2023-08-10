#include "file_io.h"
#include "profiler.h"
#include <cassert>
#include <fstream>
#include <filesystem>
#include <SDL_filesystem.h>

namespace R3
{
	namespace FileIO
	{
		std::string c_baseDirectory = "";
		std::string c_exeDirectory = "";

		void InitialisePaths()
		{
			c_baseDirectory = std::filesystem::current_path().concat("//").string();
			c_exeDirectory = std::string(SDL_GetBasePath()) + "\\";
		}

		const std::string_view GetBasePath()
		{
			return c_baseDirectory;
		}

		bool SaveTextToFile(std::string_view filePath, const std::string& src)
		{
			R3_PROF_EVENT();
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
			resultBuffer.clear();
			std::ifstream fileStream(fileSrcPath.data(), std::ios::in);
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
			std::ifstream fileStream(fileSrcPath.data(), std::ios::binary | std::ios::in);
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