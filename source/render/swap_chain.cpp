#include "swap_chain.h"
#include "device.h"
#include "window.h"
#include "core/profiler.h"
#include "core/log.h"
#include "vulkan_helpers.h"
#include <vector>
#include <SDL_vulkan.h>

namespace R3
{
	using VulkanHelpers::CheckResult;

	struct SwapchainDescriptor
	{
		VkSurfaceCapabilitiesKHR m_caps = {};
		std::vector<VkSurfaceFormatKHR> m_formats;
		std::vector<VkPresentModeKHR> m_presentModes;
	};

	SwapchainDescriptor GetSwapchainInfo(VkPhysicalDevice& physDevice, VkSurfaceKHR surface)
	{
		R3_PROF_EVENT();
		SwapchainDescriptor desc;
		CheckResult(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physDevice, surface, &desc.m_caps));

		uint32_t formats = 0;
		CheckResult(vkGetPhysicalDeviceSurfaceFormatsKHR(physDevice, surface, &formats, nullptr));
		desc.m_formats.resize(formats);
		CheckResult(vkGetPhysicalDeviceSurfaceFormatsKHR(physDevice, surface, &formats, desc.m_formats.data()));

		uint32_t presentModes = 0;
		CheckResult(vkGetPhysicalDeviceSurfacePresentModesKHR(physDevice, surface, &presentModes, nullptr));
		desc.m_presentModes.resize(presentModes);
		CheckResult(vkGetPhysicalDeviceSurfacePresentModesKHR(physDevice, surface, &presentModes, desc.m_presentModes.data()));

		return desc;
	}

	VkSurfaceFormatKHR GetSwapchainSurfaceFormat(const SwapchainDescriptor& sd)
	{
		R3_PROF_EVENT();
		for (const auto& f : sd.m_formats)
		{
			// We would prefer bgra8_srg format
			if (f.format == VK_FORMAT_B8G8R8A8_SRGB && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
			{
				return f;
			}
		}
		if (sd.m_formats.size() > 0)	// but anything else will do
		{
			return sd.m_formats[0];
		}
		else
		{
			LogError("Failed to find a suitable surface format");
			return {};
		}
	}

	VkPresentModeKHR GetSwapchainSurfacePresentMode(const SwapchainDescriptor& sd, bool enableVsync)
	{
		R3_PROF_EVENT();
		for (const auto& mode : sd.m_presentModes)
		{
			// vsync if available + asked for
			if (enableVsync && mode == VK_PRESENT_MODE_FIFO_KHR)
			{
				return mode;
			}
			// preferable, doesn't wait for vsync but avoids tearing by copying previous frame
			if (!enableVsync && mode == VK_PRESENT_MODE_MAILBOX_KHR)
			{
				return mode;
			}
		}
		// regular vsync
		return VK_PRESENT_MODE_FIFO_KHR;
	}

	VkExtent2D GetSwapchainSurfaceExtents(Window& window, const SwapchainDescriptor& sd)
	{
		R3_PROF_EVENT();
		VkExtent2D extents;
		int w = 0, h = 0;
		SDL_Vulkan_GetDrawableSize(window.GetHandle(), &w, &h);
		extents.width = glm::clamp((uint32_t)w, sd.m_caps.minImageExtent.width, sd.m_caps.maxImageExtent.width);
		extents.height = glm::clamp((uint32_t)h, sd.m_caps.minImageExtent.height, sd.m_caps.maxImageExtent.height);
		return extents;
	}

	Swapchain::Swapchain()
	{
	}

	Swapchain::~Swapchain()
	{
	}

	bool Swapchain::Initialise(Device& d, Window& w, bool enableVsync)
	{
		R3_PROF_EVENT();

		// find what swap chains are supported
		SwapchainDescriptor swapChainSupport = GetSwapchainInfo(d.GetPhysicalDevice().m_device, d.GetMainSurface());

		// find a good combination of format/present mode
		VkSurfaceFormatKHR bestFormat = GetSwapchainSurfaceFormat(swapChainSupport);
		VkPresentModeKHR bestPresentMode = GetSwapchainSurfacePresentMode(swapChainSupport, enableVsync);
		VkExtent2D extents = GetSwapchainSurfaceExtents(w, swapChainSupport);

		// We generally want min image count + 1 to avoid stalls. Apparently
		uint32_t imageCount = glm::min(swapChainSupport.m_caps.minImageCount + 1, swapChainSupport.m_caps.maxImageCount);
		VkSwapchainCreateInfoKHR scInfo = { 0 };
		scInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		scInfo.flags = 0;
		scInfo.surface = d.GetMainSurface();
		scInfo.minImageCount = imageCount;
		scInfo.imageFormat = bestFormat.format;
		scInfo.imageColorSpace = bestFormat.colorSpace;
		scInfo.imageExtent = extents;
		scInfo.imageArrayLayers = 1;	// 1 unless doing vr
		scInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;	// image used as colour attachment and transfer dest
		scInfo.preTransform = swapChainSupport.m_caps.currentTransform;
		scInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;	// we dont care about alpha from window system
		scInfo.presentMode = bestPresentMode;
		scInfo.clipped = VK_TRUE;		// yes we want clipping

		auto qfi = VulkanHelpers::FindQueueFamilyIndices(d.GetPhysicalDevice(), d.GetMainSurface());
		if (qfi.m_graphicsIndex == qfi.m_presentIndex)	// do we have more than 1 queue?
		{
			// An image is owned by one queue family at a time and ownership must be explicitly transferred before using it in another queue family
			// Since we only have one queue family it is safe
			// If we had more, we would need to do queue family transfers... apparently
			scInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		}
		else
		{
			//  Images can be used across multiple queue families without explicit ownership transfers.
			//	VK_SHARING_MODE_CONCURRENT specifies that concurrent access to any range or image subresource of the object from multiple queue families is supported.
			//  (as long as the queue families match these ones)
			scInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
			scInfo.queueFamilyIndexCount = 2;
			scInfo.pQueueFamilyIndices = reinterpret_cast<const uint32_t*>(&qfi);	// gross
		}

		VkResult r = vkCreateSwapchainKHR(d.GetVkDevice(), &scInfo, nullptr, &m_swapChain);
		if (!CheckResult(r))
		{
			LogError("Failed to create swap chain");
			return false;
		}

		// Get all the images created as part of the swapchain
		uint32_t actualImageCount = 0;
		CheckResult(vkGetSwapchainImagesKHR(d.GetVkDevice(), m_swapChain, &actualImageCount, nullptr));
		m_swapChainImages.resize(actualImageCount);
		CheckResult(vkGetSwapchainImagesKHR(d.GetVkDevice(), m_swapChain, &actualImageCount, m_swapChainImages.data()));
		m_swapChainExtents = extents;
		m_swapChainFormat = bestFormat;
		m_swapChainPresentMode = bestPresentMode;

		// Create image views for every image in the swap chain
		m_swapChainImageViews.resize(actualImageCount);
		for (uint32_t i = 0; i < actualImageCount; ++i)
		{
			VkImageViewCreateInfo createView = { 0 };
			createView.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			createView.image = m_swapChainImages[i];
			createView.flags = 0;
			createView.viewType = VK_IMAGE_VIEW_TYPE_2D;	// we want a view to a 2d image
			createView.format = bestFormat.format;			// match the format of the swap chain image
			createView.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;	// no data swizzling pls
			createView.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
			createView.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
			createView.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
			createView.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;	// which parts of the image do we want to be visible
			createView.subresourceRange.baseMipLevel = 0;
			createView.subresourceRange.levelCount = 1;		// mip count (we only have 1 for swap images)
			createView.subresourceRange.baseArrayLayer = 0;	// no array textures here
			createView.subresourceRange.layerCount = 1;		// array texture counts
			VkResult r = vkCreateImageView(d.GetVkDevice(), &createView, nullptr, &m_swapChainImageViews[i]);
			if (!CheckResult(r))
			{
				LogError("Failed to create image view for swap image {}", i);
				return false;
			}
		}

		return true;
	}

	void Swapchain::Destroy(Device& d)
	{
		R3_PROF_EVENT();

		for (auto imageView : m_swapChainImageViews)
		{
			vkDestroyImageView(d.GetVkDevice(), imageView, nullptr);
		}
		m_swapChainImageViews.clear();
		m_swapChainImages.clear();	// destroyed as part of swap chain
		vkDestroySwapchainKHR(d.GetVkDevice(), m_swapChain, nullptr);
	}
}