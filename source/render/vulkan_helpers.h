#pragma once

#include "core/glm_headers.h"
#include <vulkan/vulkan_core.h>
#include <vk_mem_alloc.h>
#include <vector>
#include <string>
#include <string_view>
#include <functional>

struct VmaAllocator_T;
namespace R3
{
	class Window;
	class CommandBufferAllocator;

	struct AllocatedBuffer
	{
		VmaAllocation m_allocation = {};
		VkBuffer m_buffer = {};
	};

	// our aim is NOT to abstract away vulkan, just hide the annoying bits
	namespace VulkanHelpers
	{
		namespace Extensions
		{
			extern PFN_vkCmdBeginDebugUtilsLabelEXT m_vkCmdBeginDebugUtilsLabelEXT;
			extern PFN_vkCmdEndDebugUtilsLabelEXT m_vkCmdEndDebugUtilsLabelEXT;
			extern PFN_vkSetDebugUtilsObjectNameEXT m_vkSetDebugUtilsObjectNameEXT;

			bool Initialise(VkDevice);
		};

		// scoped object that annotates a region of a command buffer with a name/colour
		class CommandBufferRegionLabel
		{
		public:
			CommandBufferRegionLabel(VkCommandBuffer cmds, std::string_view label, glm::vec4 colour = { 0,0,0,0 });
			~CommandBufferRegionLabel();
			CommandBufferRegionLabel(CommandBufferRegionLabel&&) = delete;
			CommandBufferRegionLabel(const CommandBufferRegionLabel&) = delete;
		private:
			VkCommandBuffer m_cmds;
		};

		// Attach a name to a vulkan object
		void SetBufferName(VkDevice, AllocatedBuffer&, std::string_view name);
		void SetShaderName(VkDevice, VkShaderModule, std::string_view name);
		void SetCommandBufferName(VkDevice, VkCommandBuffer, std::string_view name);
		void SetImageName(VkDevice, VkImage, std::string_view name);

		// Checks result, outputs any errors, crashes if fatal
		bool CheckResult(const VkResult& r);

		// Run cmds immediately on a particular queue, wait for the results via fence
		// Useful for copying data on transfer queues, debugging, etc
		bool RunCommandsImmediate(VkDevice d, VkQueue cmdQueue, VkCommandPool cmdPool, VkFence waitFence, std::function<void(VkCommandBuffer&)> fn);

		// Images
		bool BlitColourImageToImage(VkCommandBuffer cmds, VkImage srcImage, VkExtent2D srcSize, VkImage destImage, VkExtent2D destSize, VkFilter filter = VK_FILTER_LINEAR);
		VkImageCreateInfo CreateImage2DNoMSAANoMips(VkFormat format, VkImageUsageFlags usageFlags, VkExtent3D extents);
		VkImageCreateInfo CreateImage2DNoMSAA(VkFormat format, VkImageUsageFlags usageFlags, VkExtent3D extents, uint32_t mipLevels);
		VkImageViewCreateInfo CreateImageView2DNoMSAANoMips(VkFormat format, VkImage image, VkImageAspectFlags aspectFlags);
		VkImageViewCreateInfo CreateImageView2DNoMSAA(VkFormat format, VkImage image, VkImageAspectFlags aspectFlags, uint32_t mipLevels);

		// Buffers
		VkDeviceAddress GetBufferDeviceAddress(VkDevice device, const AllocatedBuffer& b);
		AllocatedBuffer CreateBuffer(VmaAllocator vma, uint64_t sizeBytes, VkBufferUsageFlags usage, VmaMemoryUsage memUsage = VMA_MEMORY_USAGE_AUTO, uint32_t allocFlags = 0);

		// ShaderModule wraps the spirv and can be deleted once a pipeline is built with it
		VkShaderModule CreateShaderModule(VkDevice device, const std::vector<uint8_t>& srcSpirv);
		VkShaderModule LoadShaderModule(VkDevice device, std::string_view filePath);
		
		// Pipeline stuff
		VkPipelineViewportStateCreateInfo CreatePipelineDynamicViewportState(int viewportCount = 1, int scissorCount = 1);
		VkPipelineVertexInputStateCreateInfo CreatePipelineEmptyVertexInputState();
		VkPipelineShaderStageCreateInfo CreatePipelineShaderState(VkShaderStageFlagBits stage, VkShaderModule shader, const char* entryPoint = "main");
		VkPipelineDynamicStateCreateInfo CreatePipelineDynamicState(const std::vector<VkDynamicState>& statesToEnable);
		VkPipelineInputAssemblyStateCreateInfo CreatePipelineInputAssemblyState(VkPrimitiveTopology topology, bool enablePrimitiveRestart=false);
		VkPipelineMultisampleStateCreateInfo CreatePipelineMultiSampleState_SingleSample();
		VkPipelineRasterizationStateCreateInfo CreatePipelineRasterState(VkPolygonMode polyMode, VkCullModeFlags cullMode, VkFrontFace frontFace);
		VkPipelineColorBlendAttachmentState CreatePipelineColourBlendAttachment_NoBlending();
		VkPipelineColorBlendAttachmentState CreatePipelineColourBlendAttachment_AlphaBlending();
		VkPipelineColorBlendStateCreateInfo CreatePipelineColourBlendState(const std::vector<VkPipelineColorBlendAttachmentState>& attachments);
		VkPipeline CreateComputePipeline(VkDevice device, VkShaderModule shader, VkPipelineLayout layout, std::string_view entryPoint = "main");

		// Synchronisation helpers
		VkImageMemoryBarrier MakeImageBarrier(VkImage image, VkImageAspectFlags aspectFlags, VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask, VkImageLayout oldLayout, VkImageLayout newLayout);
		VkImageMemoryBarrier MakeImageBarrier(VkImage image, uint32_t miplevels, VkImageAspectFlags aspectFlags, VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask, VkImageLayout oldLayout, VkImageLayout newLayout);
		void DoMemoryBarrier(VkCommandBuffer cmds, VkPipelineStageFlags2 srcStage, VkAccessFlags2 srcAccess, VkPipelineStageFlags2 destStage, VkAccessFlags2 destAccess);

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
			bool m_bindlessSupported = false;
		};
		std::vector<PhysicalDeviceDescriptor> EnumeratePhysicalDevices(VkInstance& instance);
		int ChooseGraphicsPhysicalDevice(const std::vector<PhysicalDeviceDescriptor>& devices, VkSurfaceKHR surface);	// return -1 if none found

		struct QueueFamilyIndices
		{
			uint32_t m_graphicsIndex = -1;	// supports both graphics AND synchronous compute!
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