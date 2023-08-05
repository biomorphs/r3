#include "render_system.h"
#include "window.h"
#include "vulkan/vulkan.h"
#include <SDL.h>
#include <SDL_vulkan.h>
#include <fmt/format.h>

namespace R3
{
	static constexpr bool c_validationLayersEnabled = true;

	struct RenderSystem::VkStuff
	{
		VkInstance m_vkInstance;
	};

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
			fmt::print("Failed to create window... {}", SDL_GetError());
			return false;
		}

		if (!CreateVkInstance())
		{
			fmt::print("Failed to create VK instance");
			return false;
		}

		m_mainWindow->Show();

		return true;
	}

	void RenderSystem::Shutdown()
	{
		m_mainWindow->Hide();
		vkDestroyInstance(m_vk->m_vkInstance, nullptr);
		m_mainWindow = nullptr;
	}

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

	bool RenderSystem::CreateVkInstance()
	{
		VkApplicationInfo appInfo;
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

		VkInstanceCreateInfo createInfo{};
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