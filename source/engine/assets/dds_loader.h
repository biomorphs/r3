#pragma once
#include <string_view>
#include <optional>

namespace R3
{
	namespace Textures {
		struct TextureData;
	}
	std::optional<Textures::TextureData> LoadTexture_DDS(std::string_view path);
}