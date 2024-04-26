#pragma once
#include <vulkan/vulkan.h>
#include <vector>

namespace R3
{
	class Device;
	class Window;

	// Represents the actual swap chain images + views
	class Swapchain
	{
	public:
		Swapchain();
		~Swapchain();
		bool Initialise(Device& d, Window& w);
		void Destroy(Device& d);

		VkExtent2D GetExtents() { return m_swapChainExtents; }
		std::vector<VkImage>& GetImages() { return m_swapChainImages; }
		std::vector<VkImageView>& GetViews() { return m_swapChainImageViews; }
		VkSwapchainKHR& GetSwapchain() { return m_swapChain; }
		VkSurfaceFormatKHR& GetFormat() { return m_swapChainFormat; }

	private:
		VkSwapchainKHR m_swapChain = VK_NULL_HANDLE;
		VkExtent2D m_swapChainExtents = {};
		VkSurfaceFormatKHR m_swapChainFormat = {};
		VkPresentModeKHR m_swapChainPresentMode = {};
		std::vector<VkImage> m_swapChainImages;
		std::vector<VkImageView> m_swapChainImageViews;
	};
}