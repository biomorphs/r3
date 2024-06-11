#pragma once
#include <string>
#include <optional>
#include <vector>

namespace R3
{
	namespace Textures
	{
		enum class Format 
		{
			R_U8,
			RG_U8,
			RGB_U8,						// danger not really supported
			RGBA_U8,
			RGBA_BC7
		};
		std::string_view FormatToString(Format f);

		struct TextureData
		{
			uint32_t m_width = 0;
			uint32_t m_height = 0;
			Format m_format = Format::RGBA_U8;
			struct ImageData {
				size_t m_offset = 0;		// offset into m_imgData
				size_t m_sizeBytes = 0;
			};
			std::vector<ImageData> m_mips;
			std::vector<uint8_t> m_imgData;
		};

		// Helpers for loading + baking textures	
		std::string GetBakedTexturePath(std::string_view pathName);

		// Load a texture
		std::optional<TextureData> LoadTexture(std::string_view pathName);

		// Bake a texture file, blocks until completion
		// output writes to GetBakedTexturePath(path)
		bool BakeTexture(std::string_view pathName);

		// Calculate the size in bytes of 1 mip level given a size and format
		uint64_t GetMipSizeBytes(uint32_t w, uint32_t h, Format f);
	}
}