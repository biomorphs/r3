#pragma once

#include "engine/systems.h"
#include "engine/texture_handle.h"
#include "core/mutex.h"
#include <concurrentqueue/concurrentqueue.h>

namespace R3
{
	class TextureSystem : public System
	{
	public:
		static std::string_view GetName() { return "Textures"; }
		virtual void RegisterTickFns();

		TextureHandle LoadTexture(std::string path);

	private:
		struct TextureDesc {
			std::string m_name;	// can be a path or a user-defined name
			uint32_t m_width = 0;
			uint32_t m_height = 0;
			uint32_t m_channels = 0;
		};
		struct LoadedTexture
		{
			TextureHandle m_destination;
			std::vector<uint8_t> m_data;
			uint32_t m_width;
			uint32_t m_height;
			uint32_t m_channels;
		};

		bool ProcessLoadedTextures();
		bool ShowGui();
		bool LoadTextureInternal(std::string_view path, TextureHandle targetHandle);
		TextureHandle FindExistingMatchingName(std::string name);	// locks the mutex

		moodycamel::ConcurrentQueue<std::unique_ptr<LoadedTexture>> m_loadedTextures;

		Mutex m_texturesMutex;
		std::vector<TextureDesc> m_textures;

		bool m_showGui = false;
	};
}