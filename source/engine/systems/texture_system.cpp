#include "texture_system.h"
#include "job_system.h"
#include "engine/asset_file.h"
#include "engine/imgui_menubar_helper.h"
#include "render/vulkan_helpers.h"
#include "render/render_system.h"
#include "render/buffer_pool.h"
#include "render/device.h"
#include "render/descriptors.h"
#include "core/profiler.h"
#include "core/file_io.h"
#include "core/log.h"
#include <vulkan/vulkan.h>
#include <imgui.h>
#include <imgui_impl_vulkan.h>
#include <cassert>
#include <algorithm>

// STB IMAGE linkage
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ASSERT(x) assert(x)
#include "external/stb_image/stb_image.h"

namespace R3
{
	constexpr uint32_t c_currentVersion = 0;
	constexpr uint32_t c_minVersionSupported = 0;

	struct TextureSystem::TextureDesc 
	{
		std::string m_name;	// can be a path or a user-defined name
		uint32_t m_width = 0;
		uint32_t m_height = 0;
		uint32_t m_channels = 0;
		uint32_t m_miplevels = 1;
		VkImage m_image = VK_NULL_HANDLE;
		VmaAllocation m_allocation = nullptr;
		VkImageView m_imageView = VK_NULL_HANDLE;
		VkDescriptorSet m_imGuiDescSet = VK_NULL_HANDLE;	// used to draw textures via imgui
	};

	struct TextureSystem::LoadedTexture
	{
		TextureHandle m_destination;
		uint32_t m_width = 0;
		uint32_t m_height = 0;
		uint32_t m_channels = 0;
		uint32_t m_miplevels = 1;						// if > 1, mips will be generated on gpu after load
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
		render->m_onShutdownCbs.AddCallback([this](Device& d) {
			Shutdown(d);
		});
	}

	TextureSystem::~TextureSystem()
	{
	}

	TextureSystem::TextureSystem()
	{
	}

	std::string TextureSystem::GetBakedAssetPath(std::string_view pathName)
	{
		// get the source path relative to data base directory
		std::string relPath = FileIO::SanitisePath(pathName);

		// replace any directory separators with '_'
		std::replace(relPath.begin(), relPath.end(), '/', '_');
		std::replace(relPath.begin(), relPath.end(), '\\', '_');

		// add our own extension
		relPath += ".btex";

		// use the temp directory for baked data
		std::string bakedPath = std::string(FileIO::GetBasePath()) + "\\baked\\" + relPath;
		return std::filesystem::absolute(bakedPath).string();
	}

	std::optional<AssetFile> TextureSystem::LoadSourceAsset(std::string_view path, uint32_t componentCount)
	{
		R3_PROF_EVENT();
		AssetFile newFile;
		stbi_set_flip_vertically_on_load_thread(true);
		int w = 0, h = 0, components = 0;	// components is ignored (we override it)
		unsigned char* rawData;
		{
			R3_PROF_EVENT("stbi_load");	// stbi_loadf to load floating point, or stbi_load_16 for half float
			rawData = stbi_load(path.data(), &w, &h, &components, componentCount);
			if (rawData == nullptr)
			{
				LogError("Failed to load texture file '{}'", path);
				return {};
			}
		}
		newFile.m_header["AssetType"] = "Texture";
		newFile.m_header["Version"] = c_currentVersion;
		newFile.m_header["SourceFile"] = path;
		newFile.m_header["Width"] = w;
		newFile.m_header["Height"] = h;
		newFile.m_header["Channels"] = componentCount;
		newFile.m_header["PixelType"] = "u8";
		AssetFile::Blob dataBlob;
		dataBlob.m_name = "ImageData";
		dataBlob.m_data.resize(w * h * componentCount);
		{
			R3_PROF_EVENT("Copy Blob");
			memcpy(dataBlob.m_data.data(), rawData, w * h * componentCount);
		}
		newFile.m_blobs.push_back(std::move(dataBlob));

		stbi_image_free(rawData);

		return newFile;
	}

	std::string_view TextureSystem::GetTextureName(const TextureHandle& t)
	{
		ScopedLock lock(m_texturesMutex);
		if (t.m_index != -1 && t.m_index < m_textures.size())
		{
			return m_textures[t.m_index].m_name;
		}
		return "invalid";
	}

	glm::ivec2 TextureSystem::GetTextureDimensions(const TextureHandle& t)
	{
		ScopedLock lock(m_texturesMutex);
		if (t.m_index != -1 && t.m_index < m_textures.size())
		{
			return { m_textures[t.m_index].m_width, m_textures[t.m_index].m_height };
		}
		return {-1,-1};
	}

	uint32_t TextureSystem::GetTextureChannels(const TextureHandle& t)
	{
		ScopedLock lock(m_texturesMutex);
		if (t.m_index != -1 && t.m_index < m_textures.size())
		{
			return m_textures[t.m_index].m_channels;
		}
		return -1;
	}

	uint64_t TextureSystem::GetTextureGpuSizeBytes(const TextureHandle& t)
	{
		ScopedLock lock(m_texturesMutex);
		if (t.m_index != -1 && t.m_index < m_textures.size())			// only valid for uncompressed textures
		{
			const auto& tt = m_textures[t.m_index];
			uint32_t imgWidth = tt.m_width;
			uint32_t imgHeight = tt.m_height;
			size_t sizeBytes = imgWidth * imgHeight * tt.m_channels;	// top mip 
			for (uint32_t mip = 1; mip < tt.m_miplevels; ++mip)
			{
				if (imgWidth > 1) imgWidth /= 2;
				if (imgHeight > 1) imgHeight /= 2;
				sizeBytes += imgWidth * imgHeight * tt.m_channels;
			}
			return sizeBytes;
		}
		return 0;
	}

	uint64_t TextureSystem::GetTotalGpuMemoryUsedBytes()
	{
		ScopedLock lock(m_texturesMutex);
		uint64_t sizeBytes = 0;
		for (int tt = 0; tt < m_textures.size(); ++tt)
		{
			sizeBytes += GetTextureGpuSizeBytes(TextureHandle(tt));
		}
		return sizeBytes;
	}

	VkDescriptorSet_T* TextureSystem::GetTextureImguiSet(const TextureHandle& t)
	{
		ScopedLock lock(m_texturesMutex);
		if (t.m_index != -1 && t.m_index < m_textures.size())
		{
			return m_textures[t.m_index].m_imGuiDescSet;
		}
		return nullptr;
	}

	bool TextureSystem::WriteAllTextureDescriptors(VkCommandBuffer_T* buf)
	{
		R3_PROF_EVENT();
		if (m_textures.size() == 0 || m_allTexturesSet == nullptr)
		{
			return false;
		}
		VkImageView defaultTexture = m_textures[0].m_imageView;
		if (defaultTexture == 0)
		{
			return false;	// tmp, can't risk not setting a image view
		}
		std::vector<VkDescriptorImageInfo> imageInfos(m_textures.size());
		std::vector< VkWriteDescriptorSet> writes(m_textures.size());
		for (int i = 0; i < m_textures.size(); ++i)
		{
			VkDescriptorImageInfo newImgInfo = {};
			newImgInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			newImgInfo.imageView = m_textures[i].m_imageView ? m_textures[i].m_imageView : defaultTexture;	// todo, proper bindless makes this go away
			newImgInfo.sampler = m_defaultSampler;	// todo, want to separate samplers from textures...
			imageInfos[i] = newImgInfo;

			VkWriteDescriptorSet writeTextures = {};
			writeTextures.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writeTextures.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			writeTextures.descriptorCount = 1;
			writeTextures.dstArrayElement = i;
			writeTextures.dstBinding = 0;
			writeTextures.dstSet = m_allTexturesSet;
			writeTextures.pImageInfo = &imageInfos[i];
			writes[i] = writeTextures;
		}
		if (writes.size() > 0)
		{
			auto render = GetSystem<RenderSystem>();
			vkUpdateDescriptorSets(render->GetDevice()->GetVkDevice(), (uint32_t)writes.size(), writes.data(), 0, nullptr);
		}
		return writes.size() > 0;
	}

	VkDescriptorSetLayout_T* TextureSystem::GetDescriptorsLayout()
	{
		return m_allTexturesDescriptorLayout;
	}

	VkDescriptorSet_T* TextureSystem::GetAllTexturesSet()
	{
		return m_allTexturesSet;
	}

	bool TextureSystem::Init()
	{
		R3_PROF_EVENT();

		auto render = GetSystem<RenderSystem>();

		// create default sampler
		VkSamplerCreateInfo sampler = {};
		sampler.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		sampler.magFilter = VK_FILTER_LINEAR;
		sampler.minFilter = VK_FILTER_LINEAR;
		sampler.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		sampler.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		sampler.maxLod = VK_LOD_CLAMP_NONE;
		sampler.minLod = 0;
		if (!VulkanHelpers::CheckResult(vkCreateSampler(render->GetDevice()->GetVkDevice(), &sampler, nullptr, &m_defaultSampler)))
		{
			LogError("Failed to create sampler");
			return false;
		}

		DescriptorLayoutBuilder layoutBuilder;
		layoutBuilder.AddBinding(0, c_maxTextures, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);		// one array of all textures
		m_allTexturesDescriptorLayout = layoutBuilder.Create(*render->GetDevice(), true);			// true = enable bindless
		if(m_allTexturesDescriptorLayout==nullptr)
		{
			LogError("Failed to create descriptor set layout for textures");
			return false;
		}

		m_descriptorAllocator = std::make_unique<DescriptorSetSimpleAllocator>();
		std::vector<VkDescriptorPoolSize> poolSizes = {
			{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, c_maxTextures }
		};
		if (!m_descriptorAllocator->Initialise(*render->GetDevice(), c_maxTextures, poolSizes))
		{
			LogError("Failed to create descriptor allocator");
			return false;
		}
		m_allTexturesSet = m_descriptorAllocator->Allocate(*render->GetDevice(), m_allTexturesDescriptorLayout);

		auto defaultTexture = LoadTexture("common/textures/white_4x4.png");
		if (defaultTexture.m_index != 0)
		{
			LogWarn("Default texture did not get index 0!");	// not the end of the world
		}

		return true;
	}

	TextureHandle TextureSystem::LoadTexture(std::string path, uint32_t componentCount, bool mipsEnabled)
	{
		R3_PROF_EVENT();
		auto actualPath = FileIO::SanitisePath(path);
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
		if (m_textures.size() + 1 > c_maxTextures)
		{
			LogWarn("Max texture handles reached");
			return TextureHandle::Invalid();
		}

		// make a new handle entry even if the load fails
		// (we want to return a usable handle regardless)
		TextureHandle newHandle;
		{
			ScopedLock lock(m_texturesMutex);
			m_textures.push_back({ actualPath });
			newHandle = TextureHandle{ static_cast<uint32_t>(m_textures.size() - 1) };
		}
		m_descriptorsNeedUpdate = true;	// ensure a new entry is written to the descriptor set (or gpu will crash if it tries to read an unset one)

		// push a job to load the texture data
		auto loadTextureJob = [actualPath, newHandle, this, componentCount, mipsEnabled]()
		{
			char debugName[1024] = { '\0' };
			sprintf_s(debugName, "LoadTexture %s", actualPath.c_str());
			R3_PROF_EVENT_DYN(debugName);
			if (!LoadTextureInternal(actualPath, componentCount, mipsEnabled && m_generateMips, newHandle))
			{
				LogError("Failed to load texture {}", actualPath);
			}
			m_texturesLoading--;
		};
		m_texturesLoading++;
		GetSystem<JobSystem>()->PushJob(JobSystem::SlowJobs, loadTextureJob);

		return newHandle;
	}

	void TextureSystem::Shutdown(Device& d)
	{
		R3_PROF_EVENT();
		while (m_texturesLoading > 0)	// wait for all texture loads to finish
		{
			GetSystem<JobSystem>()->ProcessJobImmediate(JobSystem::SlowJobs);
		}

		{
			ScopedLock lock(m_texturesMutex);
			for (int t = 0; t < m_textures.size(); ++t)
			{
				if (m_textures[t].m_imGuiDescSet != VK_NULL_HANDLE)
				{
					ImGui_ImplVulkan_RemoveTexture(m_textures[t].m_imGuiDescSet);
				}
				vkDestroyImageView(d.GetVkDevice(), m_textures[t].m_imageView, nullptr);
				vmaDestroyImage(d.GetVMA(), m_textures[t].m_image, m_textures[t].m_allocation);
			}
		}
		vkDestroySampler(d.GetVkDevice(), m_defaultSampler, nullptr);
		m_descriptorAllocator = {};
		vkDestroyDescriptorSetLayout(d.GetVkDevice(), m_allTexturesDescriptorLayout, nullptr);
	}

	// assumes all mips have been transitioned to dst_optimal
	void TextureSystem::GenerateMipsFromTopMip(Device& d, VkCommandBuffer_T* cmdBuffer, LoadedTexture& t)
	{
		R3_PROF_EVENT();

		VkImageMemoryBarrier barrier{};	// reuse this after each tranfer
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.image = t.m_image;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		barrier.subresourceRange.baseArrayLayer = 0;
		barrier.subresourceRange.layerCount = 1;
		barrier.subresourceRange.levelCount = 1;
		int32_t mipWidth = t.m_width;
		int32_t mipHeight = t.m_height;
		for (uint32_t i = 1; i < t.m_miplevels; i++) 
		{
			// transition the src mip to transfer_src
			barrier.subresourceRange.baseMipLevel = i - 1;
			barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
			vkCmdPipelineBarrier(cmdBuffer,
				VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
				0, nullptr,
				0, nullptr,
				1, &barrier);

			// now use blit to write the textures
			VkImageBlit blit{};
			blit.srcOffsets[0] = { 0, 0, 0 };
			blit.srcOffsets[1] = { mipWidth, mipHeight, 1 };
			blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			blit.srcSubresource.mipLevel = i - 1;
			blit.srcSubresource.baseArrayLayer = 0;
			blit.srcSubresource.layerCount = 1;
			blit.dstOffsets[0] = { 0, 0, 0 };
			blit.dstOffsets[1] = { mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1 };
			blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			blit.dstSubresource.mipLevel = i;
			blit.dstSubresource.baseArrayLayer = 0;
			blit.dstSubresource.layerCount = 1;
			vkCmdBlitImage(cmdBuffer,
				t.m_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				t.m_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				1, &blit,
				VK_FILTER_LINEAR);	// note linear filter may not be supported for all texture formats

			if (mipWidth > 1) mipWidth /= 2;
			if (mipHeight > 1) mipHeight /= 2;
		}
	}

	bool TextureSystem::ProcessLoadedTextures(Device& d, VkCommandBuffer_T* cmdBuffer)
	{
		R3_PROF_EVENT();
		auto render = GetSystem<RenderSystem>();
		ScopedLock lock(m_texturesMutex);
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
			dst.m_miplevels = t->m_miplevels;

			// transition all mips to VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
			auto transferbarrier = VulkanHelpers::MakeImageBarrier(dst.m_image,dst.m_miplevels,
				VK_IMAGE_ASPECT_COLOR_BIT,
				VK_ACCESS_NONE,
				VK_ACCESS_TRANSFER_WRITE_BIT,
				VK_IMAGE_LAYOUT_UNDEFINED,
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
			);
			// VK_PIPELINE_STAGE_TRANSFER_BIT = any time before transfers run
			vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &transferbarrier);

			// now copy from staging to the final image mip 0
			VkBufferImageCopy copyRegion = {};
			copyRegion.bufferOffset = 0;
			copyRegion.bufferRowLength = 0;
			copyRegion.bufferImageHeight = 0;
			copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			copyRegion.imageSubresource.mipLevel = 0;
			copyRegion.imageSubresource.baseArrayLayer = 0;
			copyRegion.imageSubresource.layerCount = 1;
			copyRegion.imageExtent = { dst.m_width, dst.m_height, 1 };

			// copy the buffer into the image
			vkCmdCopyBufferToImage(cmdBuffer, t->m_stagingBuffer.m_buffer.m_buffer, dst.m_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
				&copyRegion);

			if (dst.m_miplevels > 1)
			{
				GenerateMipsFromTopMip(d, cmdBuffer, *t);
			}

			// now transition all mip levels to VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL after the transfer completes + before fragment shader
			auto readbarrier = VulkanHelpers::MakeImageBarrier(dst.m_image, dst.m_miplevels,
				VK_IMAGE_ASPECT_COLOR_BIT,
				VK_ACCESS_TRANSFER_WRITE_BIT,
				VK_ACCESS_SHADER_READ_BIT,
				VK_IMAGE_LAYOUT_UNDEFINED,
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
			);
			vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &readbarrier);

			// todo, update global textures descriptor set ready for drawing

			dst.m_imGuiDescSet = ImGui_ImplVulkan_AddTexture(m_defaultSampler, dst.m_imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
			render->GetStagingBufferPool()->Release(t->m_stagingBuffer);
			m_descriptorsNeedUpdate = true;	// update the descriptors now
		}

		// for now just write all descriptors each frame
		if (m_descriptorsNeedUpdate)
		{
			WriteAllTextureDescriptors(cmdBuffer);
			m_descriptorsNeedUpdate = false;
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
				ImGui::Checkbox("Runtime mip generation", &m_generateMips);
				uint64_t totalMemUsed = GetTotalGpuMemoryUsedBytes();
				double totalMemoryMb = (uint64_t)totalMemUsed / (1024.0 * 1024.0);
				std::string txt = std::format("{:.3f}mb gpu memory used", totalMemoryMb);
				ImGui::Text(txt.c_str());
				ScopedTryLock lock(m_texturesMutex);
				if (lock.IsLocked())
				{
					txt = std::format("{} textures loaded", m_textures.size());
					ImGui::Text(txt.c_str());
					for (int ti=0;ti<m_textures.size();++ti)
					{
						auto& t = m_textures[ti];
						if (t.m_imGuiDescSet != VK_NULL_HANDLE)
						{
							ImVec2 size((float)t.m_width * 0.25f, (float)t.m_height * 0.25f);
							ImGui::Image(t.m_imGuiDescSet, size);
						}
						auto sizeBytes = GetTextureGpuSizeBytes(TextureHandle(ti));
						double sizeMb = (double)sizeBytes / (1024.0 * 1024.0);
						totalMemoryMb += sizeMb;
						txt = std::format("{} ({}x{}x{} - {:.3f}mb)", t.m_name, t.m_width, t.m_height, t.m_channels, sizeMb);
						ImGui::SeparatorText(txt.c_str());
					}
				}
			}
			ImGui::End();
		}
		return true;
	}

	bool TextureSystem::LoadTextureInternal(std::string_view path, uint32_t componentCount, bool generateMips, TextureHandle targetHandle)
	{
		R3_PROF_EVENT();
		auto render = GetSystem<RenderSystem>();
		auto device = render->GetDevice();
		auto srcTexture = LoadSourceAsset(path, componentCount);
		if (!srcTexture)
		{
			LogError("Failed to load source texture {}", path);
			return false;
		}
		// get a staging buffer + copy the data to it
		const size_t stagingSize = srcTexture->m_blobs[0].m_data.size();
		auto stagingBuffer = render->GetStagingBufferPool()->GetBuffer(stagingSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_AUTO);
		if (!stagingBuffer.has_value())
		{
			LogError("Failed to get staging buffer of size {}", stagingSize);
			return false;
		}
		{
			R3_PROF_EVENT("CopyToStaging");
			memcpy(stagingBuffer->m_mappedBuffer, srcTexture->m_blobs[0].m_data.data(), stagingSize);
		}
		
		auto loadedData = std::make_unique<LoadedTexture>();
		loadedData->m_destination = targetHandle;
		loadedData->m_width = srcTexture->m_header["Width"];
		loadedData->m_height = srcTexture->m_header["Height"];
		loadedData->m_channels = srcTexture->m_header["Channels"];
		loadedData->m_stagingBuffer = std::move(*stagingBuffer);
		if (generateMips)
		{
			loadedData->m_miplevels = static_cast<uint32_t>(std::floor(std::log2(std::max(loadedData->m_width, loadedData->m_height)))) + 1;
		}
		// Create the vulkan image and image-view now
		VkImageUsageFlags usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		if (generateMips)
		{
			usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;	// mips are copied from this texture
		}
		VkExtent3D extents = {
			(uint32_t)loadedData->m_width, (uint32_t)loadedData->m_height, 1
		};
		VkFormat format;
		switch (loadedData->m_channels)
		{
		case 1:
			format = VK_FORMAT_R8_UNORM;
			break;
		case 2:
			format = VK_FORMAT_R8G8_UNORM;
			break;
		case 4:
			format = VK_FORMAT_R8G8B8A8_UNORM;
			break;
		default:
			LogError("Unsupported component count {}", loadedData->m_channels);
			return false;
		}

		auto imageCreateInfo = VulkanHelpers::CreateImage2DNoMSAA(format, usage, extents, loadedData->m_miplevels);
		VmaAllocationCreateInfo allocInfo = { };
		allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
		allocInfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);	// fast gpu memory
		auto r = vmaCreateImage(device->GetVMA(), &imageCreateInfo, &allocInfo, &loadedData->m_image, &loadedData->m_allocation, nullptr);
		if (!VulkanHelpers::CheckResult(r))
		{
			LogError("Failed to allocate memory for texture {}", path);
			return false;
		}
		auto viewCreateInfo = VulkanHelpers::CreateImageView2DNoMSAA(format, loadedData->m_image, VK_IMAGE_ASPECT_COLOR_BIT, loadedData->m_miplevels);
		r = vkCreateImageView(device->GetVkDevice(), &viewCreateInfo, nullptr, &loadedData->m_imageView);
		if (!VulkanHelpers::CheckResult(r))
		{
			LogError("Failed to create image view for texture {}", path);
			return false;
		}
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