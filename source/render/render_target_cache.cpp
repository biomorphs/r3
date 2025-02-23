#include "render_target_cache.h"
#include "render_system.h"
#include "device.h"
#include "core/log.h"
#include "core/profiler.h"

namespace R3
{
	bool RenderTargetInfo::operator==(const RenderTargetInfo& a)
	{
		return a.m_name == m_name &&
			a.m_format == m_format &&
			a.m_layers == m_layers &&
			a.m_levels == m_levels &&
			a.m_samples == m_samples &&
			a.m_size == m_size &&
			a.m_sizeType == m_sizeType &&
			a.m_usageFlags == m_usageFlags &&
			a.m_aspectFlags == m_aspectFlags;
	}

	void RenderTargetCache::Clear()
	{
		R3_PROF_EVENT();
		auto render = Systems::GetSystem<RenderSystem>();
		for (int i = 0; i < m_allTargets.size(); ++i)
		{
			if (m_allTargets[i].m_allocation != nullptr)	// owned by the cache
			{
				vkDestroyImageView(render->GetDevice()->GetVkDevice(), m_allTargets[i].m_view, nullptr);
				vmaDestroyImage(render->GetDevice()->GetVMA(), m_allTargets[i].m_image, m_allTargets[i].m_allocation);
			}
		}
		m_allTargets.clear();
	}

	void RenderTargetCache::ResetForNewFrame()
	{
		R3_PROF_EVENT();
		for (int i = 0; i < m_allTargets.size(); ++i)
		{
			m_allTargets[i].m_lastAccessMode = VK_ACCESS_2_NONE;
			m_allTargets[i].m_lastStageFlags = VK_PIPELINE_STAGE_2_NONE;
			m_allTargets[i].m_lastLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		}
	}

	RenderTargetCache::RenderTargetCache()
	{
		m_allTargets.reserve(1024);	// some large number to ensure the pointers dont get invalidated
	}

	RenderTargetCache::~RenderTargetCache()
	{
		Clear();
	}

	glm::vec2 RenderTargetCache::GetTargetSize(const RenderTargetInfo& info)
	{
		if (info.m_sizeType == RenderTargetInfo::SizeType::Fixed)
		{
			return info.m_size;
		}
		else
		{
			auto render = Systems::GetSystem<RenderSystem>();
			return render->GetWindowExtents() * info.m_size;
		}
	}

	void RenderTargetCache::AddTarget(const RenderTargetInfo& info, VkImage image, VkImageView view)
	{
		R3_PROF_EVENT();
		auto found = std::find_if(m_allTargets.begin(), m_allTargets.end(), [&](RenderTarget& t)
		{
			return t.m_info.m_name == info.m_name;
		});
		if (found != m_allTargets.end())
		{
			*found = {};
			found->m_info = info;
			found->m_image = image;
			found->m_view = view;
		}
		else
		{
			RenderTarget newTarget;
			newTarget.m_info = info;
			newTarget.m_image = image;
			newTarget.m_view = view;
			m_allTargets.push_back(newTarget);
		}
	}

	RenderTarget* RenderTargetCache::GetTarget(const RenderTargetInfo& info)
	{
		R3_PROF_EVENT();
		auto found = std::find_if(m_allTargets.begin(), m_allTargets.end(), [&](RenderTarget& t)
		{
			return t.m_info.m_name == info.m_name;
		});
		if (found != m_allTargets.end())
		{
			return &(*found);
		}
		auto render = Systems::GetSystem<RenderSystem>();
		auto newImgSize = GetTargetSize(info);
		VkExtent3D extents = { (uint32_t)newImgSize.x, (uint32_t)newImgSize.y, 1};
		VkImage newImage = VK_NULL_HANDLE;
		VmaAllocation newAllocation = nullptr;
		VkImageCreateInfo imgCreateInfo = VulkanHelpers::CreateImage2DNoMSAANoMips(info.m_format, info.m_usageFlags, extents);
		VmaAllocationCreateInfo allocInfo = { };
		allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;		// We want the allocation to be in fast gpu memory!
		allocInfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		auto r = vmaCreateImage(render->GetDevice()->GetVMA(), &imgCreateInfo, &allocInfo, &newImage, &newAllocation, nullptr);
		if (!VulkanHelpers::CheckResult(r))
		{
			LogError("Failed to create new attachment image");
			return nullptr;
		}
		VulkanHelpers::SetImageName(render->GetDevice()->GetVkDevice(), newImage, info.m_name);
		// Create an ImageView for the image so it can be used in shaders
		VkImageView newView = VK_NULL_HANDLE;
		VkImageViewCreateInfo vci = VulkanHelpers::CreateImageView2DNoMSAANoMips(info.m_format, newImage, info.m_aspectFlags);
		r = vkCreateImageView(render->GetDevice()->GetVkDevice(), &vci, nullptr, &newView);
		if (!VulkanHelpers::CheckResult(r))
		{
			LogError("Failed to create new attachment image view");
			return nullptr;
		}
		
		// Add it to the cache + we are done
		RenderTarget newTarget;
		newTarget.m_info = info;
		newTarget.m_image = newImage;
		newTarget.m_allocation = newAllocation;
		newTarget.m_view = newView;
		m_allTargets.push_back(newTarget);
		return &m_allTargets[m_allTargets.size()-1];
	}

	void RenderTargetCache::EnumerateTargets(EnumerateTargetFn fn)
	{
		R3_PROF_EVENT();
		auto render = Systems::GetSystem<RenderSystem>();
		for (const auto target : m_allTargets)
		{
			size_t allocSizeBytes = 0;
			if (target.m_allocation != nullptr)
			{
				VmaAllocationInfo allocInfo = {0};
				vmaGetAllocationInfo(render->GetDevice()->GetVMA(), target.m_allocation, &allocInfo);
				allocSizeBytes = allocInfo.size;
			}
			else
			{
				size_t pixelSizeBytes = 0;
				switch (target.m_info.m_format)
				{
				case VK_FORMAT_B8G8R8A8_SRGB:
					pixelSizeBytes = 4;
					break;
				default:
					pixelSizeBytes = 0;
				}
				glm::vec2 dims = GetTargetSize(target.m_info);
				allocSizeBytes = (size_t)dims.x * (size_t)dims.y * pixelSizeBytes;
			}
			fn(target.m_info, allocSizeBytes);
		}
	}
}