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

	RenderTargetCache::RenderTargetCache()
	{
		m_allTargets.reserve(1024);	// some large number to ensure the pointers dont get invalidated
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
		auto found = std::find_if(m_allTargets.begin(), m_allTargets.end(), [&](RenderTarget& t)
		{
			return t.m_info.m_name == info.m_name;	// risky?
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
}