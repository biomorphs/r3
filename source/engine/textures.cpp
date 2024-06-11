#include "textures.h"
#include "engine/asset_file.h"
#include "core/file_io.h"
#include "core/log.h"
#include "core/profiler.h"
#include <stb_image.h>
#include <algorithm>
#include <filesystem>

namespace R3
{
	namespace Textures
	{
		std::string GetBakedTexturePath(std::string_view pathName)
		{
			// get the source path relative to data base directory
			std::string relPath = FileIO::SanitisePath(pathName);
			if (relPath.size() == 0)
			{
				LogWarn("Texture file {} is outside data root", pathName);
				return {};
			}

			// replace any directory separators with '_'
			std::replace(relPath.begin(), relPath.end(), '/', '_');
			std::replace(relPath.begin(), relPath.end(), '\\', '_');

			// add our own extension
			relPath += ".dds";

			// use the temp directory for baked data
			std::string bakedPath = std::string(FileIO::GetBasePath()) + "\\baked\\" + relPath;
			return std::filesystem::absolute(bakedPath).string();
		}

		std::optional<TextureData> LoadTexture(std::string_view pathName)
		{
			R3_PROF_EVENT();
			stbi_set_flip_vertically_on_load_thread(true);
			int w = 0, h = 0, srcComponents = 0;
			if (stbi_info(pathName.data(), &w, &h, &srcComponents) == 0)
			{
				LogWarn("Failed to get texture info from {}", pathName);
				return {};
			}
			if (srcComponents == 3)
			{
				LogInfo("Loading RGB image {} as RGBA", pathName);	// rgb is not supported on vast majority of gpus
				srcComponents = 4;
			}
			unsigned char* rawData = nullptr;
			{
				R3_PROF_EVENT("stbi_load");	// stbi_loadf to load floating point, or stbi_load_16 for half float
				int components = 0;			// ignored
				rawData = stbi_load(pathName.data(), &w, &h, &components, srcComponents);
				if (rawData == nullptr)
				{
					LogError("Failed to load texture file '{}'", pathName);
					return {};
				}
			}
			TextureData newTexture;
			newTexture.m_width = w;
			newTexture.m_height = h;
			switch (srcComponents)
			{
			case 1:
				newTexture.m_format = Textures::Format::R_U8;
				break;
			case 2:
				newTexture.m_format = Textures::Format::RG_U8;
				break;
			case 3:
				newTexture.m_format = Textures::Format::RGB_U8;
				break;
			case 4:
				newTexture.m_format = Textures::Format::RGBA_U8;
				break;
			default:
				LogInfo("Unupported channel count {}", srcComponents);
				break;
			}
			size_t totalSize = w * h * srcComponents;
			newTexture.m_imgData.resize(totalSize);
			memcpy(newTexture.m_imgData.data(), rawData, totalSize);
			newTexture.m_mips.emplace_back(0, totalSize);
			stbi_image_free(rawData);
			return newTexture;
		}

		uint64_t GetMipSizeBytes(uint32_t w, uint32_t h, Format f)
		{
			switch (f)
			{
			case Format::R_U8:
				return w * h;
			case Format::RG_U8:
				return w * h * 2;
			case Format::RGB_U8:
				return w * h * 3;
			case Format::RGBA_U8:
				return w * h * 4;
			case Format::RGBA_BC7:
				return w * h;	// 16 bytes per block of 4x4 texels = 1 byte per pixel
			default:
				return 0;
			}
		}
	}
}