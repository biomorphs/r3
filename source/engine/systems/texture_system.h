#pragma once

#include "engine/systems.h"
#include "engine/texture_handle.h"
#include "core/mutex.h"
#include "core/glm_headers.h"
#include <concurrentqueue/concurrentqueue.h>
#include <optional>

struct VkCommandBuffer_T;
struct VkSampler_T;
struct VkDescriptorSetLayout_T;
struct VkPipelineLayout_T;
struct VkDescriptorSet_T;
namespace R3
{
	namespace Textures
	{
		enum class Format;
	}
	class DescriptorSetSimpleAllocator;
	class Device;
	class AssetFile;
	class TextureSystem : public System
	{
	public:
		TextureSystem();
		virtual ~TextureSystem();
		static std::string_view GetName() { return "Textures"; }
		virtual void RegisterTickFns();
		virtual bool Init();
		
		TextureHandle LoadTexture(std::string path, bool mipsEnabled = true);
		std::string_view GetTextureName(const TextureHandle& t);
		glm::ivec2 GetTextureDimensions(const TextureHandle& t);
		uint64_t GetTextureGpuSizeBytes(const TextureHandle& t);
		Textures::Format GetTextureFormat(const TextureHandle& t);
		VkDescriptorSet_T* GetTextureImguiSet(const TextureHandle& t);
		uint64_t GetTotalGpuMemoryUsedBytes();

		VkDescriptorSetLayout_T* GetDescriptorsLayout();				// used to create pipelines that accept the array of textures
		VkDescriptorSet_T* GetAllTexturesSet();

	private:
		struct TextureDesc;
		struct LoadedTexture;

		bool LoadTextureInternal(std::string_view path, bool generateMips, TextureHandle targetHandle);
		void GenerateMipsFromTopMip(Device& d, VkCommandBuffer_T* cmdBuffer, LoadedTexture& t);
		bool WriteAllTextureDescriptors(VkCommandBuffer_T* buf);
		TextureHandle FindExistingMatchingName(std::string name);	// locks the mutex
		void Shutdown(Device& d);
		bool ProcessLoadedTextures(Device& d, VkCommandBuffer_T* cmdBuffer);
		bool ShowGui();

		moodycamel::ConcurrentQueue<std::unique_ptr<LoadedTexture>> m_loadedTextures;
		std::atomic<int> m_texturesLoading = 0;

		const uint32_t c_maxTextures = 1024;

		Mutex m_texturesMutex;
		std::vector<TextureDesc> m_textures;

		VkSampler_T* m_defaultSampler = nullptr;
		VkDescriptorSetLayout_T* m_allTexturesDescriptorLayout = nullptr;
		std::unique_ptr<DescriptorSetSimpleAllocator> m_descriptorAllocator;
		VkDescriptorSet_T* m_allTexturesSet = nullptr;	// the global set (bindless!)
		bool m_descriptorsNeedUpdate = false;
		bool m_generateMips = true;
		bool m_showGui = false;
	};
}