#pragma once

#include "vulkan_helpers.h"
#include <vulkan/vulkan.h>	// there is no point trying to hide it
#include <vector>
#include <memory>

struct VmaAllocator_T;
namespace R3
{
	class Window;

	// Hooks up a window with vulkan
	// owns the physical device, logical device, window surface
	class Device
	{
	public:
		Device() = delete;
		Device(const Device&) = delete;
		Device(Device&&) = delete;
		Device(Window* w);
		~Device();
		bool Initialise(bool enableDynamicRendering = true);

		VkInstance& GetVkInstance() { return m_vkInstance; }
		VulkanHelpers::PhysicalDeviceDescriptor& GetPhysicalDevice() { return m_physicalDevice; }
		VkDevice& GetVkDevice() { return m_device; };
		VmaAllocator_T* GetVMA() { return m_vma; }
		VkSurfaceKHR& GetMainSurface() { return m_mainSurface; }
		VkQueue& GetGraphicsQueue() { return m_graphicsQueue; }
		VkQueue& GetPresentQueue() { return m_presentQueue; }
	private:
		bool CreatePhysicalDevice();
		bool CreateLogicalDevice();
		bool CreateSurface();
		bool InitialiseVMA();

		void Destroy();

		VulkanHelpers::CreateVkInstanceParams m_createInstanceParams;
		Window* m_window = nullptr;
		VkInstance m_vkInstance = VK_NULL_HANDLE;
		VkSurfaceKHR m_mainSurface = nullptr;	// a surface that can be used to draw to the window
		VulkanHelpers::PhysicalDeviceDescriptor m_physicalDevice;
		VkDevice m_device = VK_NULL_HANDLE;
		VkQueue m_graphicsQueue = VK_NULL_HANDLE;
		VkQueue m_presentQueue = VK_NULL_HANDLE;
		VmaAllocator_T* m_vma = nullptr;	// vulkan memory allocator
	};
}