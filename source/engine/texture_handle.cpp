#include "texture_handle.h"
#include "serialiser.h"
#include "engine/systems/texture_system.h"

namespace R3
{
	void TextureHandle::SerialiseJson(JsonSerialiser& s)
	{
		static auto textures = Systems::GetSystem<TextureSystem>();
		if (s.GetMode() == JsonSerialiser::Write)
		{
			std::string pathName(textures->GetTextureName(*this));
			s("PathName", pathName);
		}
		else
		{
			std::string pathName;
			s("PathName", pathName);
			*this = textures->LoadTexture(pathName);
		}
	}
}