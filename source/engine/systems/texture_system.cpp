#include "texture_system.h"
#include "job_system.h"
#include "engine/imgui_menubar_helper.h"
#include "render/vulkan_helpers.h"
#include "render/render_system.h"
#include "render/buffer_pool.h"
#include "render/device.h"
#include "core/profiler.h"
#include "core/file_io.h"
#include "core/log.h"
#include <imgui.h>
#include <cassert>

// STB IMAGE linkage
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ASSERT(x) assert(x)
#include "external/stb_image/stb_image.h"

namespace R3
{
	struct TextureSystem::TextureDesc 
	{
		std::string m_name;	// can be a path or a user-defined name
		uint32_t m_width = 0;
		uint32_t m_height = 0;
		uint32_t m_channels = 0;
		VkImage m_image = VK_NULL_HANDLE;
		VmaAllocation m_allocation = nullptr;
		VkImageView m_imageView = VK_NULL_HANDLE;
	};

	struct TextureSystem::LoadedTexture
	{
		TextureHandle m_destination;
		uint32_t m_width = 0;
		uint32_t m_height = 0;
		uint32_t m_channels = 0;
		PooledBuffer m_stagingBuffer;
		VkImage m_image = VK_NULL_HANDLE;
		VmaAllocation m_allocation = nullptr;
		VkImageView m_imageView = VK_NULL_HANDLE;
	};

	void TextureSystem::RegisterTickFns()
	{
		R3_PROF_EVENT();
		RegisterTick("Textures::ShowGui", [this]() {
			return ShowGui();
		});

		auto render = GetSystem<RenderSystem>();
		render->m_onMainPassBegin.AddCallback([this](Device& d, VkCommandBuffer_T* cmdBuffer) {
			ProcessLoadedTextures(d, cmdBuffer);
		});
	}

	TextureSystem::~TextureSystem()
	{
	}

	TextureSystem::TextureSystem()
	{
	}

	TextureHandle TextureSystem::LoadTexture(std::string path)
	{
		R3_PROF_EVENT();
		auto actualPath = path;	// FileIO::SanitisePath(path);
		if (actualPath.empty())
		{
			return TextureHandle::Invalid();
		}

		// check for existence of this texture...
		TextureHandle foundHandle = FindExistingMatchingName(actualPath);
		if (foundHandle.m_index != -1)
		{
			return foundHandle;
		}

		// make a new handle entry even if the load fails
		// (we want to return a usable handle regardless)
		TextureHandle newHandle;
		{
			ScopedLock lock(m_texturesMutex);
			m_textures.push_back({ actualPath });
			newHandle = TextureHandle{ static_cast<uint32_t>(m_textures.size() - 1) };
		}

		// push a job to load the texture data
		auto loadTextureJob = [actualPath, newHandle, this]()
		{
			char debugName[1024] = { '\0' };
			sprintf_s(debugName, "LoadTexture %s", actualPath.c_str());
			R3_PROF_EVENT_DYN(debugName);
			if (!LoadTextureInternal(actualPath, newHandle))
			{
				LogError("Failed to load texture {}", actualPath);
			}
			m_texturesLoading--;
		};
		m_texturesLoading++;
		GetSystem<JobSystem>()->PushJob(JobSystem::SlowJobs, loadTextureJob);

		return TextureHandle();
	}

	void TextureSystem::Shutdown()
	{
		R3_PROF_EVENT();
		auto device = GetSystem<RenderSystem>()->GetDevice();
		while (m_texturesLoading > 0)	// wait for all texture loads to finish
		{
			GetSystem<JobSystem>()->ProcessJobImmediate(JobSystem::SlowJobs);
		}

		{
			ScopedLock lock(m_texturesMutex);
			for (int t = 0; t < m_textures.size(); ++t)
			{
				vkDestroyImageView(device->GetVkDevice(), m_textures[t].m_imageView, nullptr);
				vmaDestroyImage(device->GetVMA(), m_textures[t].m_image, m_textures[t].m_allocation);
			}
		}
	}

	bool TextureSystem::ProcessLoadedTextures(Device& d, VkCommandBuffer_T* cmdBuffer)
	{
		R3_PROF_EVENT();
		auto render = GetSystem<RenderSystem>();
		ScopedLock lock(m_texturesMutex);	// is this a good idea???
		std::unique_ptr<LoadedTexture> t;
		while (m_loadedTextures.try_dequeue(t))
		{
			assert(t->m_destination.m_index != -1 && t->m_destination.m_index < m_textures.size());
			auto& dst = m_textures[t->m_destination.m_index];
			dst.m_width = t->m_width;
			dst.m_height = t->m_height;
			dst.m_channels = t->m_channels;
			dst.m_allocation = t->m_allocation;
			dst.m_image = t->m_image;
			dst.m_imageView = t->m_imageView;

			// todo, kick off transfer from staging -> actual image via graphics cmd buffer + image transition
			// pipeline barrier?
			// todo, update global textures descriptor set ready for drawing

			LogInfo("Texture {} loaded", dst.m_name);

			render->GetStagingBufferPool()->Release(t->m_stagingBuffer);
		}
		return true;
	}

	bool TextureSystem::ShowGui()
	{
		R3_PROF_EVENT();
		auto& debugMenu = MenuBar::MainMenu().GetSubmenu("Debug");
		debugMenu.AddItem("Textures", [this]() {
			m_showGui = !m_showGui;
		});
		if (m_showGui)
		{
			ImGui::Begin("Textures");
			{
				ScopedTryLock lock(m_texturesMutex);
				if (lock.IsLocked())
				{
					std::string txt = std::format("{} textures loaded", m_textures.size());
					ImGui::Text(txt.c_str());
					for (const auto& t : m_textures)
					{
						txt = std::format("{} ({}x{}x{}", t.m_name, t.m_width, t.m_height, t.m_channels);
						ImGui::Text(txt.c_str());
					}
				}
			}
			ImGui::End();
		}
		return true;
	}

	bool TextureSystem::LoadTextureInternal(std::string_view path, TextureHandle targetHandle)
	{
		R3_PROF_EVENT();
		auto render = GetSystem<RenderSystem>();
		auto device = render->GetDevice();

		stbi_set_flip_vertically_on_load(true);		// do we need this with vulkan?

		// load as 8-bit RGBA image by default
		//	we need to call stbi_loadf to load floating point, or stbi_load_16 for half float
		int w, h, components;	// components is ignored, we always load rgba
		unsigned char* rawData = stbi_load(path.data(), &w, &h, &components, 4);
		if (rawData == nullptr)
		{
			LogError("Failed to load texture file '{}'", path);
			return false;
		}
		// Todo (maybe) - generate mips on cpu?

		// get a staging buffer + copy the data to it
		auto loadedData = std::make_unique<LoadedTexture>();
		const size_t stagingSize = w * h * 4 * sizeof(uint8_t);
		auto stagingBuffer = render->GetStagingBufferPool()->GetBuffer(stagingSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_AUTO);
		if (!stagingBuffer.has_value())
		{
			LogError("Failed to get staging buffer of size {}", stagingSize);
			return false;
		}
		{
			R3_PROF_EVENT("CopyToStaging");
			memcpy(stagingBuffer->m_mappedBuffer, rawData, stagingSize);
		}
		stbi_image_free(rawData);
		loadedData->m_stagingBuffer = std::move(*stagingBuffer);
		
		// Create the vulkan image and image-view now
		VkImageUsageFlags usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		VkExtent3D extents = {
			(uint32_t)w, (uint32_t)h, 1
		};
		auto imageCreateInfo = VulkanHelpers::CreateImage2DNoMSAANoMips(VK_FORMAT_R8G8B8A8_UNORM, usage, extents);
		VmaAllocationCreateInfo allocInfo = { };
		allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
		allocInfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);	// fast gpu memory
		auto r = vmaCreateImage(device->GetVMA(), &imageCreateInfo, &allocInfo, &loadedData->m_image, &loadedData->m_allocation, nullptr);
		if (!VulkanHelpers::CheckResult(r))
		{
			LogError("Failed to allocate memory for texture {}", path);
			return false;
		}
		auto viewCreateInfo = VulkanHelpers::CreateImageView2DNoMSAANoMips(VK_FORMAT_R8G8B8A8_UNORM, loadedData->m_image, VK_IMAGE_ASPECT_COLOR_BIT);
		r = vkCreateImageView(device->GetVkDevice(), &viewCreateInfo, nullptr, &loadedData->m_imageView);
		if (!VulkanHelpers::CheckResult(r))
		{
			LogError("Failed to create image view for texture {}", path);
			return false;
		}
		loadedData->m_destination = targetHandle;
		loadedData->m_width = w;
		loadedData->m_height = h;
		loadedData->m_channels = 4;
		m_loadedTextures.enqueue(std::move(loadedData));
		return true;
	}

	TextureHandle TextureSystem::FindExistingMatchingName(std::string name)
	{
		R3_PROF_EVENT();
		ScopedLock lock(m_texturesMutex);
		for (uint64_t i = 0; i < m_textures.size(); ++i)
		{
			if (m_textures[i].m_name == name)
			{
				TextureHandle index = { static_cast<uint32_t>(i) };
				return index;
			}
		}
		return TextureHandle();
	}
}