#include "dds_loader.h"
#include "textures.h"
#include "core/profiler.h"
#include "core/log.h"
#include "core/file_io.h"
#include "core/glm_headers.h"
#include <array>

namespace R3
{
	// All flags / struct formats are from https ://msdn.microsoft.com/en-us/library/bb943992%28v=vs.85%29.aspx
	// Note that not all flags are dealt with, only the ones we care about are listed here

	enum DDSHeaderFlags
	{
		DDSD_CAPS = 0x1,						// Required in every.dds file.	
		DDSD_HEIGHT = 0x2,						// Required in every.dds file.	
		DDSD_WIDTH = 0x4,						// Required in every.dds file.
		DDSD_PIXELFORMAT = 0x1000,				// Required in every.dds file.
		DDSD_MIPMAPCOUNT = 0x20000,				// Required in a mipmapped texture.
		DDSD_LINEARSIZE = 0x80000,				// Required when pitch is provided for a compressed texture.
	};

	enum DDSHeaderCapsFlags
	{
		DDSCAPS_TEXTURE = 0x1000				//Required for all textures, the only flag we care about
	};

	enum DDSPixelFormatFlags
	{
		DDPF_ALPHAPIXELS = 0x1,					// has alpha data
		DDPF_ALPHA = 0x2,						// ^^ (used in some older files)
		DDPF_FOURCC = 0x4,						//Texture contains compressed RGB data; 
		DDPF_RGB = 0x40,						//uncompressed RGB data
	};

	struct DDSFileHeader
	{
		uint32_t m_ddsFileToken;				// Should always be 'DDS'
		uint32_t m_headerSize;					// Should always be 124
		uint32_t m_flags;						// See DDSHeaderFlags
		uint32_t m_heightPx;					// Image height
		uint32_t m_widthPx;						// Image Width
		uint32_t m_pitchOrLinearSize;			// Pitch of uncompressed texture or total size of highest mip in compressed one
		uint32_t m_volumeDepth;					// How many slices in volume texture
		uint32_t m_mipCount;					// Mip count
		uint32_t m_reserved1[11];
		uint32_t m_pixelFormatSize;				// Should always be c_ddsPixelFormatSize
		uint32_t m_pixelFormatFlags;			// See DDSPixelFormatFlags
		uint32_t m_pixelFormatFourCC;			// FourCC of the pixel format. 
		uint32_t m_pixelFormatRGBBitCount;		// Num. bits in RGB data, we don't use it
		uint32_t m_pixelFormatRBitMask;			// Bit mask of red channel in uncompressed textures
		uint32_t m_pixelFormatGBitMask;			// Bit mask of green channel in uncompressed textures
		uint32_t m_pixelFormatBBitMask;			// Bit mask of blue channel in uncompressed textures
		uint32_t m_pixelFormatABitMask;			// Bit mask of alpha channel in uncompressed textures
		uint32_t m_capsFlags;					// See DDSHeaderCapsFlags
		uint32_t m_cubeVolumeFlags;				// Cubemap / volume texture flags
		uint32_t m_unusedFlags3;
		uint32_t m_unusedFlags4;
		uint32_t m_reserved2;
	};

	// see https://learn.microsoft.com/en-us/windows/win32/api/dxgiformat/ne-dxgiformat-dxgi_format
	enum DDS_DXGI_FORMATS
	{
		DXGI_FORMAT_UNKNOWN = 0,
		DXGI_FORMAT_BC4_UNORM = 80,
		DXGI_FORMAT_BC5_UNORM = 83,
		DXGI_FORMAT_BC7_UNORM = 98
	};

	// exists after header if DDS_PIXELFORMAT dwFlags is set to DDPF_FOURCC and dwFourCC is set to "DX10"
	// used for texture arrays
	struct DDSHeaderDX10Extension				
	{
		uint32_t m_dxgiFormat;							
		uint32_t resourceDimension;						// see https://learn.microsoft.com/en-us/windows/win32/api/d3d10/ne-d3d10-d3d10_resource_dimension
		uint32_t miscFlag;
		uint32_t arraySize;								// num. elements in array texture
		uint32_t miscFlags2;
	};

	static const uint32_t c_ddsFileToken = ' SDD';		// Token to mark a dds file
	static const uint32_t c_ddsHeaderSize = 124;		// Size of header struct, should always be 124
	static const uint32_t c_ddsPixelFormatSize = 32;	// Size of pixel format struct, should always be 32
	static const uint32_t c_expectedHeaderFlags = DDSD_HEIGHT | DDSD_WIDTH;

	bool ExtractHeader(const std::vector<uint8_t>& data, DDSFileHeader& header)
	{
		if (data.size() < sizeof(header))
		{
			return false;
		}
		memcpy(&header, data.data(), sizeof(header));
		// Ensure its a dds file
		if (header.m_ddsFileToken != c_ddsFileToken)
		{
			LogWarn("Not a DDS file!");
			return false;
		}
		// Ensure the header looks valid by testing the embedded struct sizes
		if (header.m_headerSize != c_ddsHeaderSize || header.m_pixelFormatSize != c_ddsPixelFormatSize)
		{
			LogWarn("Bad DDS header");
			return false;
		}

		return true;
	}

	bool IsFormatSupported(const DDSFileHeader& header, const DDSHeaderDX10Extension* dx10Ext)
	{
		if ((header.m_flags & c_expectedHeaderFlags) != c_expectedHeaderFlags)
		{
			LogWarn("Expected flags missing");
			return false;	// Unexpected header flags
		}
		if (header.m_heightPx == 0 || header.m_widthPx == 0)
		{
			LogWarn("Bad size");
			return false;
		}
		if (header.m_pitchOrLinearSize == 0)
		{
			LogWarn("Missing pitch/linear size");
			return false;	// This must be set to pull out the top mip level
		}
		if (dx10Ext != nullptr)
		{
			constexpr std::array<uint32_t,3> c_supportedFormats = {
				DXGI_FORMAT_BC4_UNORM,
				DXGI_FORMAT_BC5_UNORM,
				DXGI_FORMAT_BC7_UNORM
			};
			auto supported = std::find(c_supportedFormats.begin(), c_supportedFormats.end(), dx10Ext->m_dxgiFormat);
			if (supported == c_supportedFormats.end())
			{
				LogWarn("Unsupported DXGI format {}", dx10Ext->m_dxgiFormat);
			}
		}
		return true;
	}

	std::optional<Textures::Format> GetEngineFormat(const DDSFileHeader& header, const DDSHeaderDX10Extension* dx10Ext)
	{
		Textures::Format format = Textures::Format::RGBA_BC7;
		if (dx10Ext)
		{
			switch (dx10Ext->m_dxgiFormat)
			{
			case DXGI_FORMAT_BC4_UNORM:
				return Textures::Format::R_BC4;
			case DXGI_FORMAT_BC5_UNORM:
				return Textures::Format::RG_BC5;
			case DXGI_FORMAT_BC7_UNORM:
				return Textures::Format::RGBA_BC7;
			default:
				LogError("Unsupported format!");
				return {};
			}
		}
		else
		{
			char fourcc[5] = { 0 };
			memcpy(fourcc, &header.m_pixelFormatFourCC, sizeof(header.m_pixelFormatFourCC));
			if (strcmp(fourcc, "BC4") == 0 || strcmp(fourcc, "ATI1") == 0 || strcmp(fourcc, "BC4U") == 0)
			{
				return Textures::Format::R_BC4;
			}
			else if (strcmp(fourcc, "BC5") == 0 || strcmp(fourcc, "ATI2") == 0 || strcmp(fourcc, "BC5U") == 0)
			{
				return Textures::Format::RG_BC5;
			}
			else if (strcmp(fourcc, "BC7") == 0 || strcmp(fourcc, "BC7U") == 0)
			{
				return Textures::Format::RGBA_BC7;
			}
			else if (strcmp(fourcc, "DXT1") == 0 || strcmp(fourcc, "BC1U") == 0)
			{
				return Textures::Format::RGBA_BC1;
			}
			else if (strcmp(fourcc, "DXT2") == 0 || strcmp(fourcc, "DXT3") == 0 || strcmp(fourcc, "BC2U") == 0)
			{
				return Textures::Format::RGBA_BC2;
			}
			else if (strcmp(fourcc, "DXT4") == 0 || strcmp(fourcc, "DXT5") == 0 || strcmp(fourcc, "BC3U") == 0)
			{
				return Textures::Format::RGBA_BC3;
			}
			else
			{
				LogError("Unknown format '{}'", fourcc);
				return {};
			}
		}
		return format;
	}

	size_t GetBlockSizeBytes(Textures::Format f)
	{
		switch (f)
		{
		case Textures::Format::RGBA_BC1:
		case Textures::Format::R_BC4:
			return 8;
		case Textures::Format::RGBA_BC2:
		case Textures::Format::RGBA_BC3:
		case Textures::Format::RG_BC5:
		case Textures::Format::RGBA_BC7:
			return 16;
		default:
			assert(!"Woah");
			return 0;
		}
	}

	std::optional<Textures::TextureData> LoadTexture_DDS(std::string_view path)
	{
		R3_PROF_EVENT();

		std::vector<uint8_t> buffer;
		if (!FileIO::LoadBinaryFile(path, buffer))
		{
			return {};
		}
		DDSFileHeader header;
		if (!ExtractHeader(buffer, header))
		{
			return {};
		}
		char fourcc[5] = { 0 };
		memcpy(fourcc, &header.m_pixelFormatFourCC, sizeof(header.m_pixelFormatFourCC));
		const DDSHeaderDX10Extension* dx10Extension = nullptr;
		if (strcmp(fourcc, "DX10") == 0)
		{
			dx10Extension = reinterpret_cast<const DDSHeaderDX10Extension*>(buffer.data() + sizeof(header));
		}
		if (!IsFormatSupported(header, dx10Extension))
		{
			LogWarn("Texture is not supported");
			return {};
		}
		auto format = GetEngineFormat(header, dx10Extension);
		if (!format)
		{
			LogWarn("Texture is not supported");
			return {};
		}
		Textures::TextureData newTexture;
		newTexture.m_width = header.m_widthPx;
		newTexture.m_height = header.m_heightPx;
		newTexture.m_format = *format;
		size_t mipSrcOffset = sizeof(header) + (dx10Extension ? sizeof(*dx10Extension) : 0);
		// we should not trust the pitch output by exporters according to MS
		uint64_t imgPitch = glm::max(1u, ((header.m_widthPx + 3) / 4)) * GetBlockSizeBytes(newTexture.m_format);
		size_t mip0Size = imgPitch * newTexture.m_height / 4;

		// the image data is not aligned how we would like, so we need to remake it ourselves
		// each mip level will be stored consecutively with 16 byte alignment
		size_t maxDataSize = 0;
		size_t mipSize = mip0Size;
		for (uint32_t i = 0; i < header.m_mipCount; ++i)
		{
			maxDataSize += mipSize + 16;
			mipSize = glm::max(GetBlockSizeBytes(newTexture.m_format), mipSize / 4);
		}
		std::vector<uint8_t> imageDataAligned;
		imageDataAligned.resize(maxDataSize);
		size_t dstOffset = 0;
		size_t srcOffset = mipSrcOffset;
		mipSize = mip0Size;
		for (uint32_t i = 0; i < header.m_mipCount; ++i)
		{
			memcpy(imageDataAligned.data() + dstOffset, buffer.data() + srcOffset, mipSize);
			newTexture.m_mips.emplace_back(dstOffset, mipSize);
			dstOffset = AlignUpPow2(dstOffset + mipSize, 16ull);
			srcOffset += mipSize;
			mipSize = glm::max(GetBlockSizeBytes(newTexture.m_format), mipSize / 4);
		}
		newTexture.m_imgData = std::move(imageDataAligned);
		return newTexture;
	}
}