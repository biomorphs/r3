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

		// Load an image containing 1 u8 per component
		// set componentCount to override channel count in source
		std::optional<TextureData> LoadTexture(std::string_view pathName);

		// Calculate the size in bytes of 1 mip level given a size and format
		uint64_t GetMipSizeBytes(uint32_t w, uint32_t h, Format f);
	}

}