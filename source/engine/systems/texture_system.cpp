#include "texture_system.h"
#include "job_system.h"
#include "engine/imgui_menubar_helper.h"
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
	void TextureSystem::RegisterTickFns()
	{
		R3_PROF_EVENT();
		RegisterTick("Textures::ShowGui", [this]() {
			return ShowGui();
		});
		RegisterTick("Textures::ProcessLoaded", [this]() {
			return ProcessLoadedTextures();
		});
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
		};
		GetSystem<JobSystem>()->PushJob(JobSystem::SlowJobs, loadTextureJob);

		return TextureHandle();
	}

	bool TextureSystem::ProcessLoadedTextures()
	{
		R3_PROF_EVENT();
		ScopedLock lock(m_texturesMutex);	// is this a good idea???
		std::unique_ptr<LoadedTexture> t;
		while (m_loadedTextures.try_dequeue(t))
		{
			assert(t.m_destination.m_index != -1 && t.m_destination.m_index < m_textures.size());
			auto& dst = m_textures[t->m_destination.m_index];
			dst.m_width = t->m_width;
			dst.m_height = t->m_height;
			dst.m_channels = t->m_channels;
			LogInfo("Texture {} loaded, about to free {} bytes", dst.m_name, t->m_data.size());
			// take ownership of the ptr and delete it ourselves
			LoadedTexture* dataToFree = t.release();
			GetSystem<JobSystem>()->PushJob(JobSystem::SlowJobs, [dataToFree]() {
				R3_PROF_EVENT("DeleteLoadedTexture");
				delete dataToFree;
			});
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
		stbi_set_flip_vertically_on_load(true);		// do we need this with vulkan?

		// load as 8-bit per channel image by default
		//	we need to call stbi_loadf to load floating point, or stbi_load_16 for half float
		int w, h, components;
		unsigned char* rawData = stbi_load(path.data(), &w, &h, &components, 0);
		if (rawData == nullptr)
		{
			LogError("Failed to load texture file '{}'", path);
			return false;
		}

		// Copy the texture data to our own buffer to free up stbi memory
		const size_t imgDataSize = w * h * components * sizeof(uint8_t);
		auto loadedData = std::make_unique<LoadedTexture>();
		{
			R3_PROF_EVENT("CopyImageData");
			loadedData->m_data.resize(imgDataSize);
			memcpy(loadedData->m_data.data(), rawData, imgDataSize);
		}
		stbi_image_free(rawData);
		loadedData->m_destination = targetHandle;
		loadedData->m_width = w;
		loadedData->m_height = h;
		loadedData->m_channels = components;
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