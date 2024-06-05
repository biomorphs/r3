#pragma once

#include "engine/systems.h"
#include "engine/texture_handle.h"
#include "core/mutex.h"
#include <concurrentqueue/concurrentqueue.h>

struct VkCommandBuffer_T;
namespace R3
{
	class Device;
	class TextureSystem : public System
	{
	public:
		TextureSystem();
		virtual ~TextureSystem();
		static std::string_view GetName() { return "Textures"; }
		virtual void RegisterTickFns();
		
		TextureHandle LoadTexture(std::string path);

	private:
		struct TextureDesc;
		struct LoadedTexture;

		void Shutdown();
		bool ProcessLoadedTextures(Device& d, VkCommandBuffer_T* cmdBuffer);
		bool ShowGui();
		bool LoadTextureInternal(std::string_view path, TextureHandle targetHandle);
		TextureHandle FindExistingMatchingName(std::string name);	// locks the mutex

		moodycamel::ConcurrentQueue<std::unique_ptr<LoadedTexture>> m_loadedTextures;
		std::atomic<int> m_texturesLoading = 0;

		Mutex m_texturesMutex;
		std::vector<TextureDesc> m_textures;

		bool m_showGui = false;
	};
}