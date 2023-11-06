#pragma once

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#include <vector>
#include <string_view>
#include <functional>

struct VmaAllocator_T;
namespace R3
{
	class Window;

	struct AllocatedBuffer
	{
		VmaAllocation m_allocation = {};
		VkBuffer m_buffer = {};
	};

	// our aim is NOT to abstract away vulkan, just hide the annoying bits
	namespace VulkanHelpers
	{
		// Checks result, outputs any errors, crashes if fatal
		bool CheckResult(const VkResult& r);

		// Run cmds immediately on a particular queue, wait for the results via fence
		// Useful for copying data on transfer queues, debugging, etc
		bool RunCommandsImmediate(VkDevice d, VkQueue cmdQueue, VkCommandPool cmdPool, VkFence waitFence, std::function<void(VkCommandBuffer&)> fn);

		// Buffers
		AllocatedBuffer CreateBuffer(VmaAllocator vma, uint64_t sizeBytes, VkBufferUsageFlags usage, VmaMemoryUsage memUsage = VMA_MEMORY_USAGE_AUTO, uint32_t allocFlags = 0);

		// ShaderModule wraps the spirv and can be deleted once a pipeline is built with it
		VkShaderModule CreateShaderModule(VkDevice device, const std::vector<uint8_t>& srcSpirv);
		VkShaderModule LoadShaderModule(VkDevice device, std::string_view filePath);
		
		// Pipeline stuff
		VkPipelineShaderStageCreateInfo CreatePipelineShaderState(VkShaderStageFlagBits stage, VkShaderModule shader, const char* entryPoint = "main");
		VkPipelineDynamicStateCreateInfo CreatePipelineDynamicState(const std::vector<VkDynamicState>& statesToEnable);
		VkPipelineInputAssemblyStateCreateInfo CreatePipelineInputAssemblyState(VkPrimitiveTopology topology, bool enablePrimitiveRestart=false);
		VkPipelineMultisampleStateCreateInfo CreatePipelineMultiSampleState_SingleSample();
		VkPipelineRasterizationStateCreateInfo CreatePipelineRasterState(VkPolygonMode polyMode, VkCullModeFlags cullMode, VkFrontFace frontFace);
		VkPipelineColorBlendAttachmentState CreatePipelineColourBlendAttachment_NoBlending();
		VkPipelineColorBlendStateCreateInfo CreatePipelineColourBlendState(const std::vector<VkPipelineColorBlendAttachmentState>& attachments);

		// Synchronisation helpers
		VkImageMemoryBarrier MakeImageBarrier(VkImage image, VkImageAspectFlags aspectFlags, VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask, VkImageLayout oldLayout, VkImageLayout newLayout);

		// Initialisation helpers
		struct CreateVkInstanceParams {
			std::string m_appName = "R3";
			std::string m_engineName = "R3";
			uint32_t m_appVersion = VK_MAKE_VERSION(1, 0, 0);
			uint32_t m_engineVersion = VK_MAKE_VERSION(1, 0, 0);
			uint32_t m_vulkanApiVersion = VK_API_VERSION_1_3;
			bool m_enableValidationLayers = true;
			bool m_enableDynamicRendering = true;	// enables VK_dynamic_rendering 
		};
		VkInstance CreateVkInstance(Window& w, CreateVkInstanceParams& params);

		struct PhysicalDeviceDescriptor
		{
			VkPhysicalDevice m_device = VK_NULL_HANDLE;
			VkPhysicalDeviceProperties m_properties = {};
			VkPhysicalDeviceFeatures m_features = {};
			std::vector<VkQueueFamilyProperties> m_queues;
			std::vector<VkExtensionProperties> m_supportedExtensions;
		};
		std::vector<PhysicalDeviceDescriptor> EnumeratePhysicalDevices(VkInstance& instance);
		int ChooseGraphicsPhysicalDevice(const std::vector<PhysicalDeviceDescriptor>& devices, VkSurfaceKHR surface);	// return -1 if none found

		struct QueueFamilyIndices
		{
			uint32_t m_graphicsIndex = -1;
			uint32_t m_presentIndex = -1;
		};
		QueueFamilyIndices FindQueueFamilyIndices(const PhysicalDeviceDescriptor& pdd, VkSurfaceKHR surface);

		VkDevice CreateLogicalDevice(const PhysicalDeviceDescriptor& pdd, VkSurfaceKHR surface, bool enablyDynamicRendering = true, bool enableValidationLayers = true);

		// Swapchain stuff
		struct SwapchainDescriptor
		{
			VkSurfaceCapabilitiesKHR m_caps = {};
			std::vector<VkSurfaceFormatKHR> m_formats;
			std::vector<VkPresentModeKHR> m_presentModes;
		};
		SwapchainDescriptor GetMatchingSwapchains(const VkPhysicalDevice& physDevice, VkSurfaceKHR surface);
	}
}