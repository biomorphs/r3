#include "render_system.h"
#include "window.h"
#include "vulkan/vulkan.h"
#include <SDL.h>
#include <SDL_vulkan.h>
#include <fmt/format.h>
#include <set>

namespace R3
{
	static constexpr bool c_validationLayersEnabled = true;

	struct PhysicalDeviceDescriptor
	{
		VkPhysicalDevice m_device = VK_NULL_HANDLE;
		VkPhysicalDeviceProperties m_properties;
		VkPhysicalDeviceFeatures m_features;
		std::vector<VkQueueFamilyProperties> m_queues;
		std::vector<VkExtensionProperties> m_supportedExtensions;
	};

	struct SwapchainDescriptor
	{
		VkSurfaceCapabilitiesKHR m_caps;
		std::vector<VkSurfaceFormatKHR> m_formats;
		std::vector<VkPresentModeKHR> m_presentModes;
	};

	struct QueueFamilyIndices
	{
		uint32_t m_graphicsIndex = -1;
		uint32_t m_presentIndex = -1;
	};

	struct RenderSystem::VkStuff
	{
		VkInstance m_vkInstance;
		VkSurfaceKHR m_mainSurface = nullptr;
		PhysicalDeviceDescriptor m_physicalDevice;
		VkDevice m_device = VK_NULL_HANDLE;
		VkQueue m_graphicsQueue = VK_NULL_HANDLE;
		VkQueue m_presentQueue = VK_NULL_HANDLE;
		VkSwapchainKHR m_swapChain;
	};

	bool CheckResult(const VkResult& r)
	{
		if (r)
		{
			fmt::print("Vulkan Error! {}\n", r);
			int* crashMe = nullptr;
			*crashMe = 1;
		}
		return !r;
	}

	std::vector<VkExtensionProperties> GetSupportedInstanceExtensions() {
		std::vector<VkExtensionProperties> results;
		uint32_t extCount = 0;
		if (CheckResult(vkEnumerateInstanceExtensionProperties(nullptr, &extCount, nullptr)))	// get the extension count
		{
			results.resize(extCount);
			CheckResult(vkEnumerateInstanceExtensionProperties(nullptr, &extCount, results.data()));
		}
		return results;
	}

	bool AreExtensionsSupported(const std::vector<VkExtensionProperties>& allExtensions, std::vector<const char*> wantedExtensions)
	{
		for (const auto& r : wantedExtensions)
		{
			auto found = std::find_if(allExtensions.begin(), allExtensions.end(), [&r](const VkExtensionProperties& p) {
				return strcmp(p.extensionName, r) == 0;
			});
			if (found == allExtensions.end())
			{
				return false;
			}
		}
		return true;
	}

	std::vector<const char*> GetSDLRequiredInstanceExtensions(SDL_Window* w) {
		std::vector<const char*> results;
		uint32_t extensionCount = 0;
		if (!SDL_Vulkan_GetInstanceExtensions(w, &extensionCount, nullptr))	// first call gets count
		{
			fmt::print("SDL_Vulkan_GetInstanceExtensions failed\n");
			return results;
		}
		results.resize(extensionCount);
		if (!SDL_Vulkan_GetInstanceExtensions(w, &extensionCount, results.data()))	// first call gets count
		{
			fmt::print("SDL_Vulkan_GetInstanceExtensions failed\n");
			return results;
		}
		return results;
	}

	std::vector<VkLayerProperties> GetSupportedLayers() {
		std::vector<VkLayerProperties> result;
		uint32_t count = 0;
		if (CheckResult(vkEnumerateInstanceLayerProperties(&count, nullptr)))
		{
			result.resize(count);
			CheckResult(vkEnumerateInstanceLayerProperties(&count, result.data()));
		}

		return result;
	}

	bool AreLayersSupported(const std::vector<VkLayerProperties>& allLayers, std::vector<const char*> requestedLayers)
	{
		for (const auto& r : requestedLayers)
		{
			auto found = std::find_if(allLayers.begin(), allLayers.end(), [&r](const VkLayerProperties& p) {
				return strcmp(p.layerName, r) == 0;
				});
			if (found == allLayers.end())
			{
				return false;
			}
		}
		return true;
	}
	
	std::vector<PhysicalDeviceDescriptor> GetAllPhysicalDevices(VkInstance& instance)
	{
		std::vector<PhysicalDeviceDescriptor> results;
		uint32_t count = 0;
		std::vector<VkPhysicalDevice> allDevices;
		if (CheckResult(vkEnumeratePhysicalDevices(instance, &count, nullptr)))
		{
			allDevices.resize(count);
			CheckResult(vkEnumeratePhysicalDevices(instance, &count, allDevices.data()));
		}
		for (const auto it : allDevices)
		{
			PhysicalDeviceDescriptor newDesc;
			newDesc.m_device = it;
			vkGetPhysicalDeviceProperties(it, &newDesc.m_properties);
			vkGetPhysicalDeviceFeatures(it, &newDesc.m_features);

			uint32_t queueCount = 0;
			vkGetPhysicalDeviceQueueFamilyProperties(it, &queueCount, nullptr);
			newDesc.m_queues.resize(queueCount);
			vkGetPhysicalDeviceQueueFamilyProperties(it, &queueCount, newDesc.m_queues.data());

			uint32_t extensionCount = 0;
			vkEnumerateDeviceExtensionProperties(it, nullptr, &extensionCount, nullptr);
			newDesc.m_supportedExtensions.resize(extensionCount);
			vkEnumerateDeviceExtensionProperties(it, nullptr, &extensionCount, newDesc.m_supportedExtensions.data());

			results.push_back(newDesc);
		}
		return results;
	}

	// find indices for the queue families we care about
	QueueFamilyIndices FindQueueFamilyIndices(const PhysicalDeviceDescriptor& pdd, VkSurfaceKHR surface = VK_NULL_HANDLE)
	{
		QueueFamilyIndices qfi;
		for (int q = 0; q < pdd.m_queues.size() && (qfi.m_graphicsIndex == -1 && qfi.m_presentIndex == -1); ++q)
		{
			if (pdd.m_queues[q].queueFlags & VK_QUEUE_GRAPHICS_BIT)	// graphics queue?
			{
				qfi.m_graphicsIndex = q;
			}
			if (surface)	// can it present to the surface?
			{
				VkBool32 isSupported = false;
				VkResult r = vkGetPhysicalDeviceSurfaceSupportKHR(pdd.m_device, q, surface, &isSupported);
				if (CheckResult(r) && isSupported)
				{
					qfi.m_presentIndex = q;
				}
			}
		}
		return qfi;
	}

	RenderSystem::RenderSystem()
	{
		m_vk = std::make_unique<VkStuff>();
	}

	RenderSystem::~RenderSystem()
	{
		m_vk = nullptr;
	}

	void RenderSystem::RegisterTickFns()
	{
	}

	bool RenderSystem::Init()
	{
		if (!CreateWindow())
		{
			fmt::print("Failed to create window... {}\n", SDL_GetError());
			return false;
		}

		if (!CreateVkInstance())
		{
			fmt::print("Failed to create VK instance\n");
			return false;
		}

		if (!CreateSurface())
		{
			fmt::print("Failed to create surface\n");
			return false;
		}

		if (!CreatePhysicalDevice())
		{
			fmt::print("Failed to create physical device\n");
			return false;
		}

		if (!CreateLogicalDevice())
		{
			fmt::print("Failed to create logical device\n");
			return false;
		}

		if (!CreateSwapchain())
		{
			fmt::print("Failed to create swapchain\n");
			return false;
		}

		m_mainWindow->Show();

		return true;
	}

	void RenderSystem::Shutdown()
	{
		m_mainWindow->Hide();
		vkDestroySwapchainKHR(m_vk->m_device, m_vk->m_swapChain, nullptr);
		vkDestroyDevice(m_vk->m_device, nullptr);
		vkDestroySurfaceKHR(m_vk->m_vkInstance, m_vk->m_mainSurface, nullptr);
		vkDestroyInstance(m_vk->m_vkInstance, nullptr);
		m_mainWindow = nullptr;
	}

	SwapchainDescriptor GetSwapchainInfo(VkPhysicalDevice& physDevice, VkSurfaceKHR surface)
	{
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
			fmt::print("Failed to find a suitable surface format");
			return {};
		}
	}

	VkPresentModeKHR GetSwapchainSurfacePresentMode(const SwapchainDescriptor& sd)
	{
		for (const auto& mode : sd.m_presentModes) 
		{
			// preferable, doesn't wait for vsync but avoids tearing by copying previous frame
			if (mode == VK_PRESENT_MODE_MAILBOX_KHR)	
			{
				return mode;
			}
		}
		// regular vsync
		return VK_PRESENT_MODE_FIFO_KHR;
	}

	VkExtent2D GetSwapchainSurfaceExtents(Window& window, const SwapchainDescriptor& sd)
	{
		VkExtent2D extents;
		int w = 0, h = 0;
		SDL_Vulkan_GetDrawableSize(window.GetHandle(), &w, &h);
		extents.width = glm::clamp((uint32_t)w, sd.m_caps.minImageExtent.width, sd.m_caps.maxImageExtent.width);
		extents.height = glm::clamp((uint32_t)h, sd.m_caps.minImageExtent.height, sd.m_caps.maxImageExtent.height);
		return extents;
	}

	bool RenderSystem::CreateSwapchain()
	{
		// find what swap chains are supported
		SwapchainDescriptor swapChainSupport = GetSwapchainInfo(m_vk->m_physicalDevice.m_device, m_vk->m_mainSurface);

		// find a good combination of format/present mode
		VkSurfaceFormatKHR bestFormat = GetSwapchainSurfaceFormat(swapChainSupport);
		VkPresentModeKHR bestPresentMode = GetSwapchainSurfacePresentMode(swapChainSupport);
		VkExtent2D extents = GetSwapchainSurfaceExtents(*m_mainWindow, swapChainSupport);

		// We generally want min image count + 1 to avoid stalls. Apparently
		uint32_t imageCount = glm::min(swapChainSupport.m_caps.minImageCount + 1, swapChainSupport.m_caps.maxImageCount);
		VkSwapchainCreateInfoKHR scInfo = { 0 };
		scInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		scInfo.flags = 0;
		scInfo.surface = m_vk->m_mainSurface;
		scInfo.minImageCount = imageCount;
		scInfo.imageFormat = bestFormat.format;
		scInfo.imageColorSpace = bestFormat.colorSpace;
		scInfo.imageExtent = extents;
		scInfo.imageArrayLayers = 1;	// 1 unless doing vr
		scInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;	// image used as colour attachment
		scInfo.preTransform = swapChainSupport.m_caps.currentTransform;
		scInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;	// we dont care about alpha from window system
		scInfo.presentMode = bestPresentMode;
		scInfo.clipped = VK_TRUE;		// yes we want clipping

		auto qfi = FindQueueFamilyIndices(m_vk->m_physicalDevice, m_vk->m_mainSurface);
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

		VkResult r = vkCreateSwapchainKHR(m_vk->m_device, &scInfo, nullptr, &m_vk->m_swapChain);
		if (!CheckResult(r))
		{
			fmt::print("Failed to create swap chain\n");
			return false;
		}

		return true;
	}
	
	bool RenderSystem::CreateWindow()
	{
		Window::Properties windowProps;
		windowProps.m_sizeX = 1280;
		windowProps.m_sizeY = 720;
		windowProps.m_title = "R3";
		windowProps.m_flags = 0;
		m_mainWindow = std::make_unique<Window>(windowProps);
		return m_mainWindow != nullptr;
	}

	bool RenderSystem::CreateSurface()
	{
		return SDL_Vulkan_CreateSurface(m_mainWindow->GetHandle(), m_vk->m_vkInstance, &m_vk->m_mainSurface);
	}

	std::vector<const char*> GetRequiredDeviceExtensions()
	{
		return {
			VK_KHR_SWAPCHAIN_EXTENSION_NAME,
		};
	}

	int FindGraphicsPhysicalDevice(std::vector<PhysicalDeviceDescriptor>& allDevices, VkSurfaceKHR surface)
	{
		for (int i=0;i<allDevices.size();++i)
		{
			// is it discrete?
			if (allDevices[i].m_properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
			{
				// does it have a graphics queue and a queue that can present to the surface?
				QueueFamilyIndices qfi = FindQueueFamilyIndices(allDevices[i], surface);
				bool hasQueueFamilies = qfi.m_graphicsIndex != -1 && qfi.m_presentIndex != -1;
				bool extensionsSupported = AreExtensionsSupported(allDevices[i].m_supportedExtensions, GetRequiredDeviceExtensions());
				if (hasQueueFamilies && extensionsSupported)
				{
					// does it support a valid swap chain for the surface?
					SwapchainDescriptor swapChainSupport = GetSwapchainInfo(allDevices[i].m_device, surface);
					if (swapChainSupport.m_formats.size() > 0 && swapChainSupport.m_presentModes.size() > 0)
					{
						return i;
					}
				}
			}
		}
		return -1;
	}

	bool RenderSystem::CreateLogicalDevice()
	{
		std::vector<VkDeviceQueueCreateInfo> queues;	// which queues (and how many) do we want?
		std::vector<float> priorities;

		// create queues (we only need one per unique family index!)
		QueueFamilyIndices qfi = FindQueueFamilyIndices(m_vk->m_physicalDevice, m_vk->m_mainSurface);
		std::set<uint32_t> uniqueFamilies = { qfi.m_graphicsIndex, qfi.m_presentIndex };
		float queuePriority = 1.0f;
		for (auto it : uniqueFamilies)
		{
			assert(it != -1);
			VkDeviceQueueCreateInfo queue = { 0 };
			queue.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			queue.queueFamilyIndex = it;
			queue.queueCount = 1;
			queue.pQueuePriorities = &queuePriority;
			queues.push_back(queue);
		}

		// Which device features do we use
		VkPhysicalDeviceFeatures requiredFeatures {};

		// Create the device
		VkDeviceCreateInfo deviceCreate = { 0 };
		deviceCreate.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		deviceCreate.queueCreateInfoCount = queues.size();
		deviceCreate.pQueueCreateInfos = queues.data();

		// pass the same validation layers
		std::vector<const char*> requiredLayers;
		if constexpr (c_validationLayersEnabled)
		{
			fmt::print("Enabling validation layer\n");
			requiredLayers.push_back("VK_LAYER_KHRONOS_validation");
		}
		deviceCreate.enabledLayerCount = requiredLayers.size();
		deviceCreate.ppEnabledLayerNames = requiredLayers.data();
		std::vector<const char*> extensions = GetRequiredDeviceExtensions();
		deviceCreate.enabledExtensionCount = extensions.size();
		deviceCreate.ppEnabledExtensionNames = extensions.data();
		deviceCreate.pEnabledFeatures = &requiredFeatures;
		VkResult r = vkCreateDevice(m_vk->m_physicalDevice.m_device, &deviceCreate, nullptr, &m_vk->m_device);

		// Get ptrs to the queues now
		if (CheckResult(r))
		{
			vkGetDeviceQueue(m_vk->m_device, qfi.m_graphicsIndex, 0, &m_vk->m_graphicsQueue);
			vkGetDeviceQueue(m_vk->m_device, qfi.m_presentIndex, 0, &m_vk->m_presentQueue);
		}
		return CheckResult(r);
	}

	bool RenderSystem::CreatePhysicalDevice()
	{
		std::vector<PhysicalDeviceDescriptor> allDevices = GetAllPhysicalDevices(m_vk->m_vkInstance);
		fmt::print("All supported devices:\n");
		for (const auto& it : allDevices)
		{
			fmt::print("\t{} ({} queues)\n", it.m_properties.deviceName, it.m_queues.size());
		}
		int bestMatchIndex = FindGraphicsPhysicalDevice(allDevices, m_vk->m_mainSurface);
		if (bestMatchIndex >= 0)
		{
			m_vk->m_physicalDevice = allDevices[bestMatchIndex];
			return true;
		}

		return false;
	}

	bool RenderSystem::CreateVkInstance()
	{
		VkApplicationInfo appInfo = { 0 };
		appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		appInfo.pNext = nullptr;
		appInfo.pApplicationName = "R3";
		appInfo.pEngineName = "R3";
		appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.apiVersion = VK_API_VERSION_1_0;

		// Setup extensions
		std::vector<const char*> requiredExtensions = GetSDLRequiredInstanceExtensions(m_mainWindow->GetHandle());
		std::vector<VkExtensionProperties> supportedExtensions = GetSupportedInstanceExtensions();
		fmt::print("Supported Vulkan Extensions:\n");
		for (auto it : supportedExtensions)
		{
			fmt::print("\t{} v{}\n", it.extensionName, it.specVersion);
		}

		// Setup layers
		std::vector<VkLayerProperties> allLayers = GetSupportedLayers();
		fmt::print("Supported Layers:\n");
		for (auto it : allLayers)
		{
			fmt::print("\t{} v{} - {}\n", it.layerName, it.implementationVersion, it.description);
		}
		std::vector<const char*> requiredLayers;
		if constexpr (c_validationLayersEnabled)
		{
			fmt::print("Enabling validation layer\n");
			requiredLayers.push_back("VK_LAYER_KHRONOS_validation");
		}
		if (!AreLayersSupported(allLayers, requiredLayers))
		{
			fmt::print("Some required layers are not supported!\n");
			return false;
		}

		VkInstanceCreateInfo createInfo = {0};
		createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		createInfo.pApplicationInfo = &appInfo;
		createInfo.enabledExtensionCount = requiredExtensions.size();
		createInfo.ppEnabledExtensionNames = requiredExtensions.data();
		createInfo.enabledLayerCount = requiredLayers.size();
		createInfo.ppEnabledLayerNames = requiredLayers.data();
		VkResult result = vkCreateInstance(&createInfo, nullptr, &m_vk->m_vkInstance);
		return CheckResult(result);
	}
}