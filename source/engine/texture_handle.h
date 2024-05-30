#pragma once
#include <stdint.h>

namespace R3
{
	// Handle to a texture
	struct TextureHandle
	{
		uint32_t m_index = -1;
		static TextureHandle Invalid() { return { (uint32_t)-1 }; };
		void SerialiseJson(class JsonSerialiser& s);
	};
}