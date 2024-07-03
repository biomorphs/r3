#include "device.h"
#include "window.h"
#include "core/log.h"
#include "core/profiler.h"
#include <SDL_vulkan.h>
#include <vk_mem_alloc.h>
#include <vulkan/vk_enum_string_helper.h>

namespace R3
{
	using VulkanHelpers::CheckResult;

	Device::Device(Window* w)
		: m_window(w)
	{
	}

	Device::~Device()
	{
		Destroy();
	}

	bool Device::Initialise(bool enableValidationLayer, bool enableDynamicRendering)
	{
		m_createInstanceParams.m_enableDynamicRendering = enableDynamicRendering;
		m_createInstanceParams.m_enableValidationLayers = enableValidationLayer;
		m_vkInstance = VulkanHelpers::CreateVkInstance(*m_window, m_createInstanceParams);
		if (m_vkInstance == VK_NULL_HANDLE)
		{
			LogError("Failed to create VK instance");
			return false;
		}
		if (!CreateSurface())
		{
			LogError("Failed to create surface");
			return false;
		}

		if (!CreatePhysicalDevice())
		{
			LogError("Failed to create physical device");
			return false;
		}

		if (!CreateLogicalDevice())
		{
			LogError("Failed to create logical device");
			return false;
		}

		if (!InitialiseVMA())
		{
			LogError("Failed to initialise vulkan memory allocator");
			return false;
		}

		// Initialise the gpu profiler
		VulkanHelpers::QueueFamilyIndices qfi = VulkanHelpers::FindQueueFamilyIndices(m_physicalDevice, m_mainSurface);
		R3_PROF_GPU_INIT(&m_device, &m_physicalDevice.m_device, &m_graphicsQueue, &qfi.m_graphicsIndex, 1);

		return true;
	}

	bool Device::CreatePhysicalDevice()
	{
		R3_PROF_EVENT();
		auto allDevices = VulkanHelpers::EnumeratePhysicalDevices(m_vkInstance);
		LogInfo("All supported devices:");
		for (const auto& it : allDevices)
		{
			LogInfo("\t{} ({} queues)", it.m_properties.deviceName, it.m_queues.size());
		}
		int bestMatchIndex = VulkanHelpers::ChooseGraphicsPhysicalDevice(allDevices, m_mainSurface);
		if (bestMatchIndex >= 0)
		{
			m_physicalDevice = allDevices[bestMatchIndex];
			return true;
		}
		return false;
	}

	bool Device::CreateLogicalDevice()
	{
		R3_PROF_EVENT();

		// Create a device that supports dynamic rendering (no render passes!)
		m_device = VulkanHelpers::CreateLogicalDevice(m_physicalDevice, m_mainSurface, m_createInstanceParams.m_enableDynamicRendering, m_createInstanceParams.m_enableValidationLayers);
		if (m_device)
		{
			// get handles to the queues now
			VulkanHelpers::QueueFamilyIndices qfi = VulkanHelpers::FindQueueFamilyIndices(m_physicalDevice, m_mainSurface);
			vkGetDeviceQueue(m_device, qfi.m_graphicsIndex, 0, &m_graphicsQueue);
			vkGetDeviceQueue(m_device, qfi.m_presentIndex, 0, &m_presentQueue);
		}

		return m_device != nullptr;
	}

	bool Device::CreateSurface()
	{
		R3_PROF_EVENT();
		return SDL_Vulkan_CreateSurface(m_window->GetHandle(), m_vkInstance, &m_mainSurface);
	}

	bool Device::InitialiseVMA()
	{
		R3_PROF_EVENT();
		VmaAllocatorCreateInfo allocatorInfo = { 0 };
		allocatorInfo.physicalDevice = m_physicalDevice.m_device;
		allocatorInfo.device = m_device;
		allocatorInfo.instance = m_vkInstance;
		allocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;	// enable buffer device addresses
		return CheckResult(vmaCreateAllocator(&allocatorInfo, &m_vma));
	}

	void Device::Destroy()
	{
		R3_PROF_SHUTDOWN();

		vmaDestroyAllocator(m_vma);
		m_vma = nullptr;

		vkDestroyDevice(m_device, nullptr);
		m_device = nullptr;

		vkDestroySurfaceKHR(m_vkInstance, m_mainSurface, nullptr);
		m_mainSurface = nullptr;

		vkDestroyInstance(m_vkInstance, nullptr);
		m_vkInstance = nullptr;
	}
}