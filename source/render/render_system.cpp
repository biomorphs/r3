#include "render_system.h"
#include "window.h"
#include "vulkan/vulkan.h"
#include <SDL.h>
#include <SDL_vulkan.h>
#include <fmt/format.h>

namespace R3
{
	static constexpr bool c_validationLayersEnabled = true;

	struct PhysicalDeviceDescriptor
	{
		VkPhysicalDevice m_device = VK_NULL_HANDLE;
		VkPhysicalDeviceProperties m_properties;
		VkPhysicalDeviceFeatures m_features;
		std::vector<VkQueueFamilyProperties> m_queues;
	};

	struct RenderSystem::VkStuff
	{
		VkInstance m_vkInstance;
		PhysicalDeviceDescriptor m_physicalDevice;
		VkDevice m_device = VK_NULL_HANDLE;
		VkQueue m_graphicsQueue = VK_NULL_HANDLE;
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

	std::vector<VkExtensionProperties> GetSupportedExtensions() {
		std::vector<VkExtensionProperties> results;
		uint32_t extCount = 0;
		if (CheckResult(vkEnumerateInstanceExtensionProperties(nullptr, &extCount, nullptr)))	// get the extension count
		{
			results.resize(extCount);
			CheckResult(vkEnumerateInstanceExtensionProperties(nullptr, &extCount, results.data()));
		}
		return results;
	}

	std::vector<const char*> GetSDLRequiredExtensions(SDL_Window* w) {
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

			results.push_back(newDesc);
		}
		return results;
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

		m_mainWindow->Show();

		return true;
	}

	void RenderSystem::Shutdown()
	{
		m_mainWindow->Hide();
		vkDestroyDevice(m_vk->m_device, nullptr);
		vkDestroyInstance(m_vk->m_vkInstance, nullptr);
		m_mainWindow = nullptr;
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

	int FindPhysicalDevice(const std::vector<PhysicalDeviceDescriptor>& allDevices)
	{
		for (int i=0;i<allDevices.size();++i)
		{
			// is it discrete?
			if (allDevices[i].m_properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
			{
				// does it have a graphics queue?
				for (int q = 0; q < allDevices[i].m_queues.size(); ++q)
				{
					if ((allDevices[i].m_queues[q].queueFlags & VK_QUEUE_GRAPHICS_BIT))
					{
						return i;
					}
				}
			}
		}
		return -1;
	}

	int FindQueueFamilyIndex(const PhysicalDeviceDescriptor& pdd, uint32_t queueFlags)
	{
		for (int q=0;q<pdd.m_queues.size();++q)
		{
			if (pdd.m_queues[q].queueFlags & queueFlags)
			{
				return q;
			}
		}
		return -1;
	}

	bool RenderSystem::CreateLogicalDevice()
	{
		std::vector<VkDeviceQueueCreateInfo> queues;	// which queues (and how many) do we want?
		std::vector<float> priorities;

		// 1 Graphics queue
		int graphicsQueueIndex = FindQueueFamilyIndex(m_vk->m_physicalDevice, VK_QUEUE_GRAPHICS_BIT);
		float graphicsPriority = 1.0f;
		if (graphicsQueueIndex >= 0)
		{
			VkDeviceQueueCreateInfo graphicsQueue = { 0 };
			graphicsQueue.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			graphicsQueue.queueFamilyIndex = static_cast<uint32_t>(graphicsQueueIndex);
			graphicsQueue.queueCount = 1;
			graphicsQueue.pQueuePriorities = &graphicsPriority;
			queues.push_back(graphicsQueue);
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
		deviceCreate.enabledExtensionCount = 0;
		deviceCreate.pEnabledFeatures = &requiredFeatures;
		VkResult r = vkCreateDevice(m_vk->m_physicalDevice.m_device, &deviceCreate, nullptr, &m_vk->m_device);
		if (CheckResult(r))
		{
			vkGetDeviceQueue(m_vk->m_device, graphicsQueueIndex, 0, &m_vk->m_graphicsQueue);
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
		int bestMatchIndex = FindPhysicalDevice(allDevices);
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
		std::vector<const char*> requiredExtensions = GetSDLRequiredExtensions(m_mainWindow->GetHandle());
		std::vector<VkExtensionProperties> supportedExtensions = GetSupportedExtensions();
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