#include "vulkan_helpers.h"
#include "window.h"
#include "core/profiler.h"
#include "core/file_io.h"
#include "core/log.h"
#include <SDL_vulkan.h>
#include <vulkan/vk_enum_string_helper.h>
#include <string>
#include <cassert>
#include <set>

namespace R3
{
	namespace VulkanHelpers
	{
		bool CheckResult(const VkResult& r)
		{
			if (r)
			{
				std::string errorString = string_VkResult(r);
				LogError("Vulkan Error! {}", errorString);
				int* crashMe = nullptr;
				*crashMe = 1;
			}
			return !r;
		}

		bool RunCommandsImmediate(VkDevice d, VkQueue cmdQueue, VkCommandPool cmdPool, VkFence waitFence, std::function<void(VkCommandBuffer&)> fn)
		{
			R3_PROF_EVENT();

			// create a temporary cmd buffer from the pool
			VkCommandBufferAllocateInfo allocInfo = { 0 };
			allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
			allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
			allocInfo.commandPool = cmdPool;
			allocInfo.commandBufferCount = 1;

			VkCommandBuffer commandBuffer;
			if (!CheckResult(vkAllocateCommandBuffers(d, &allocInfo, &commandBuffer)))
			{
				LogError("Failed to create cmd buffer");
				return false;
			}

			VkCommandBufferBeginInfo beginInfo = { 0 };
			beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;	// we will only submit this buffer once
			if (!CheckResult(vkBeginCommandBuffer(commandBuffer, &beginInfo)))
			{
				LogError("Failed to begin cmd buffer");
				return false;
			}

			// run the passed fs
			fn(commandBuffer);

			CheckResult(vkEndCommandBuffer(commandBuffer));

			// submit the cmd buffer to the queue, passing the immediate fence
			VkSubmitInfo submitInfo = { 0 };
			submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
			submitInfo.commandBufferCount = 1;
			submitInfo.pCommandBuffers = &commandBuffer;

			// Submit + wait for the fence
			CheckResult(vkQueueSubmit(cmdQueue, 1, &submitInfo, waitFence));
			{
				R3_PROF_STALL("Wait for fence");
				CheckResult(vkWaitForFences(d, 1, &waitFence, true, 9999999999));
			}
			CheckResult(vkResetFences(d, 1, &waitFence));

			// We can free the cmd buffer
			vkFreeCommandBuffers(d, cmdPool, 1, &commandBuffer);

			return true;
		}

		VkImageCreateInfo CreateImage2DNoMSAANoMips(VkFormat format, VkImageUsageFlags usageFlags, VkExtent3D extents)
		{
			VkImageCreateInfo info = {};
			info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
			info.pNext = nullptr;
			info.imageType = VK_IMAGE_TYPE_2D;
			info.format = format;
			info.extent = extents;
			info.mipLevels = 1;
			info.arrayLayers = 1;
			info.samples = VK_SAMPLE_COUNT_1_BIT;
			info.tiling = VK_IMAGE_TILING_OPTIMAL;	// optimal for gpu
			info.usage = usageFlags;	// depth-stencil attachment
			return info;
		}

		VkImageViewCreateInfo CreateImageView2DNoMSAANoMips(VkFormat format, VkImage image, VkImageAspectFlags aspectFlags)
		{
			VkImageViewCreateInfo vci = {};
			vci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			vci.viewType = VK_IMAGE_VIEW_TYPE_2D;
			vci.image = image;
			vci.format = format;
			vci.subresourceRange.baseMipLevel = 0;
			vci.subresourceRange.levelCount = 1;
			vci.subresourceRange.baseArrayLayer = 0;
			vci.subresourceRange.layerCount = 1;
			vci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;	// we want to access the depth data
			return vci;
		}

		AllocatedBuffer CreateBuffer(VmaAllocator vma, uint64_t sizeBytes, VkBufferUsageFlags usage, VmaMemoryUsage memUsage, uint32_t allocFlags)
		{
			R3_PROF_EVENT();
			AllocatedBuffer newBuffer;
			VkBufferCreateInfo bci = { 0 };
			bci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
			bci.size = sizeBytes;
			bci.usage = usage;

			VmaAllocationCreateInfo allocInfo = { 0 };
			allocInfo.usage = memUsage;
			allocInfo.flags = allocFlags;

			VkResult r = vmaCreateBuffer(vma, &bci, &allocInfo, &newBuffer.m_buffer, &newBuffer.m_allocation, nullptr);
			if (!CheckResult(r))
			{
				LogError("Failed to create buffer of size {} bytes", sizeBytes);
			}
			return newBuffer;
		}

		VkShaderModule CreateShaderModule(VkDevice device, const std::vector<uint8_t>& srcSpirv)
		{
			R3_PROF_EVENT();
			assert((srcSpirv.size() % 4) == 0);	// VkShaderModuleCreateInfo wants uint32, make sure the buffer is big enough 
			VkShaderModuleCreateInfo createInfo = { 0 };
			createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
			createInfo.pCode = reinterpret_cast<const uint32_t*>(srcSpirv.data());
			createInfo.codeSize = srcSpirv.size();	// size in BYTES
			VkShaderModule shaderModule = { 0 };
			VkResult r = vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule);
			if (CheckResult(r))
			{
				return shaderModule;
			}
			else
			{
				LogError("Failed to create shader module from src spirv");
				return VkShaderModule{ 0 };
			}
		}

		VkShaderModule LoadShaderModule(VkDevice device, std::string_view filePath)
		{
			R3_PROF_EVENT();
			VkShaderModule result = VK_NULL_HANDLE;
			std::vector<uint8_t> spirv;
			if (!R3::FileIO::LoadBinaryFile(filePath, spirv))
			{
				LogError("Failed to load spirv file {}", filePath);
			}
			else
			{
				result = CreateShaderModule(device, spirv);
			}
			return result;
		}

		VkPipelineShaderStageCreateInfo CreatePipelineShaderState(VkShaderStageFlagBits stage, VkShaderModule shader, const char* entryPoint)
		{
			R3_PROF_EVENT();
			VkPipelineShaderStageCreateInfo stageInfo = { 0 };
			stageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
			stageInfo.stage = stage;
			stageInfo.module = shader;
			stageInfo.pName = entryPoint;
			return stageInfo;
		}

		VkPipelineDynamicStateCreateInfo CreatePipelineDynamicState(const std::vector<VkDynamicState>& statesToEnable)
		{
			R3_PROF_EVENT();
			VkPipelineDynamicStateCreateInfo dynamicState = { 0 };
			dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
			dynamicState.dynamicStateCount = static_cast<uint32_t>(statesToEnable.size());
			if (statesToEnable.size() > 0)
			{
				dynamicState.pDynamicStates = statesToEnable.data();
			}
			return dynamicState;
		}

		VkPipelineInputAssemblyStateCreateInfo CreatePipelineInputAssemblyState(VkPrimitiveTopology topology, bool enablePrimitiveRestart)
		{
			R3_PROF_EVENT();
			VkPipelineInputAssemblyStateCreateInfo inputAssembly = { 0 };
			inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
			inputAssembly.topology = topology;
			inputAssembly.primitiveRestartEnable = enablePrimitiveRestart;	// used with strips to break up lines/tris
			return inputAssembly;
		}

		VkPipelineMultisampleStateCreateInfo CreatePipelineMultiSampleState_SingleSample()
		{
			R3_PROF_EVENT();
			VkPipelineMultisampleStateCreateInfo multisampling = { 0 };
			multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
			multisampling.sampleShadingEnable = VK_FALSE;	// no MSAA pls
			multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;	// just 1 sample thx
			multisampling.minSampleShading = 1.0f; // Optional
			multisampling.pSampleMask = nullptr; // Optional
			multisampling.alphaToCoverageEnable = VK_FALSE; // Optional, outputs alpha to coverage for blending
			multisampling.alphaToOneEnable = VK_FALSE; // Optional, sets alpha to one for all samples
			return multisampling;
		}

		VkPipelineRasterizationStateCreateInfo CreatePipelineRasterState(VkPolygonMode polyMode, VkCullModeFlags cullMode, VkFrontFace frontFace)
		{
			R3_PROF_EVENT();
			VkPipelineRasterizationStateCreateInfo rasterizer = { 0 };
			rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
			rasterizer.depthClampEnable = VK_FALSE;		// if true fragments outsize max depth are clamped, not discarded
														// (requires enabling a device feature!)
			rasterizer.depthBiasEnable = VK_FALSE;			// no depth biasing pls
			rasterizer.lineWidth = 1.0f;					// sensible default
			rasterizer.rasterizerDiscardEnable = VK_FALSE;	// set to true to disable raster stage!
			rasterizer.polygonMode = polyMode;
			rasterizer.cullMode = cullMode;
			rasterizer.frontFace = frontFace;
			return rasterizer;
		}

		VkPipelineColorBlendAttachmentState CreatePipelineColourBlendAttachment_NoBlending()
		{
			R3_PROF_EVENT();
			VkPipelineColorBlendAttachmentState attachment = { 0 };
			attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
			attachment.blendEnable = VK_FALSE;					// No blending
			attachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
			attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
			attachment.colorBlendOp = VK_BLEND_OP_ADD; // Optional
			attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
			attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
			attachment.alphaBlendOp = VK_BLEND_OP_ADD; // Optional
			return attachment;
		}

		VkPipelineColorBlendStateCreateInfo CreatePipelineColourBlendState(const std::vector<VkPipelineColorBlendAttachmentState>& attachments)
		{
			R3_PROF_EVENT();
			VkPipelineColorBlendStateCreateInfo blending = { 0 };
			blending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
			blending.logicOpEnable = VK_FALSE;		// no logical ops on write pls
			blending.logicOp = VK_LOGIC_OP_COPY;	// Optional
			blending.attachmentCount = static_cast<uint32_t>(attachments.size());
			blending.pAttachments = attachments.data();
			blending.blendConstants[0] = 0.0f; // Optional (r component of blending constant)
			blending.blendConstants[1] = 0.0f; // Optional (g component of blending constant)
			blending.blendConstants[2] = 0.0f; // Optional (b component of blending constant)
			blending.blendConstants[3] = 0.0f; // Optional (a component of blending constant)
			return blending;
		}

		VkImageMemoryBarrier MakeImageBarrier(VkImage image, VkImageAspectFlags aspectFlags, VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask, VkImageLayout oldLayout, VkImageLayout newLayout)
		{
			VkImageMemoryBarrier barrier = { 0 };
			barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			barrier.srcAccessMask = srcAccessMask;
			barrier.dstAccessMask = dstAccessMask;
			barrier.oldLayout = oldLayout;
			barrier.newLayout = newLayout;
			barrier.image = image;

			VkImageSubresourceRange range = { 0 };
			range.aspectMask = aspectFlags;
			range.layerCount = 1;
			range.baseArrayLayer = 0;
			range.levelCount = 1;
			range.baseMipLevel = 0;
			barrier.subresourceRange = range;
			
			return barrier;
		}

		std::vector<const char*> GetSDLRequiredInstanceExtensions(SDL_Window* w)
		{
			R3_PROF_EVENT();
			std::vector<const char*> results;
			uint32_t extensionCount = 0;
			if (!SDL_Vulkan_GetInstanceExtensions(w, &extensionCount, nullptr))	// first call gets count
			{
				LogError("SDL_Vulkan_GetInstanceExtensions failed");
				return results;
			}
			results.resize(extensionCount);
			if (!SDL_Vulkan_GetInstanceExtensions(w, &extensionCount, results.data()))	// first call gets count
			{
				LogError("SDL_Vulkan_GetInstanceExtensions failed");
				return results;
			}
			return results;
		}

		std::vector<VkExtensionProperties> GetSupportedInstanceExtensions()
		{
			R3_PROF_EVENT();
			std::vector<VkExtensionProperties> results;
			uint32_t extCount = 0;
			if (CheckResult(vkEnumerateInstanceExtensionProperties(nullptr, &extCount, nullptr)))	// get the extension count
			{
				results.resize(extCount);
				CheckResult(vkEnumerateInstanceExtensionProperties(nullptr, &extCount, results.data()));
			}
			return results;
		}

		std::vector<VkLayerProperties> GetSupportedLayers()
		{
			R3_PROF_EVENT();
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
			R3_PROF_EVENT();
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

		VkInstance CreateVkInstance(Window& w, CreateVkInstanceParams& params)
		{
			R3_PROF_EVENT();
			VkApplicationInfo appInfo = { 0 };
			appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
			appInfo.pNext = nullptr;
			appInfo.pApplicationName = params.m_appName.c_str();
			appInfo.pEngineName = params.m_engineName.c_str();
			appInfo.applicationVersion = params.m_appVersion;
			appInfo.engineVersion = params.m_engineVersion;
			appInfo.apiVersion = params.m_vulkanApiVersion;

			// Setup extensions
			std::vector<const char*> requiredExtensions = GetSDLRequiredInstanceExtensions(w.GetHandle());
			std::vector<VkExtensionProperties> supportedExtensions = GetSupportedInstanceExtensions();
			LogInfo("Supported Vulkan Extensions:");
			for (auto it : supportedExtensions)
			{
				LogInfo("\t{} v{}", it.extensionName, it.specVersion);
			}

			// Setup layers
			std::vector<VkLayerProperties> allLayers = GetSupportedLayers();
			LogInfo("Supported Layers:");
			for (auto it : allLayers)
			{
				LogInfo("\t{} v{} - {}", it.layerName, it.implementationVersion, it.description);
			}
			std::vector<const char*> requiredLayers;
			if (params.m_enableValidationLayers)
			{
				LogInfo("Enabling validation layer");
				requiredLayers.push_back("VK_LAYER_KHRONOS_validation");
			}
			if (!AreLayersSupported(allLayers, requiredLayers))
			{
				LogError("Some required layers are not supported!");
				return {};
			}

			VkInstanceCreateInfo createInfo = { 0 };
			createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
			createInfo.pApplicationInfo = &appInfo;
			createInfo.enabledExtensionCount = static_cast<uint32_t>(requiredExtensions.size());
			createInfo.ppEnabledExtensionNames = requiredExtensions.data();
			createInfo.enabledLayerCount = static_cast<uint32_t>(requiredLayers.size());
			createInfo.ppEnabledLayerNames = requiredLayers.data();
			VkInstance newInstance = VK_NULL_HANDLE;
			VkResult result = vkCreateInstance(&createInfo, nullptr, &newInstance);
			CheckResult(result);
			return newInstance;
		}
		std::vector<PhysicalDeviceDescriptor> EnumeratePhysicalDevices(VkInstance& instance)
		{
			R3_PROF_EVENT();
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

		std::vector<const char*> GetRequiredDeviceExtensions()
		{
			return {
				VK_KHR_SWAPCHAIN_EXTENSION_NAME,
			};
		}

		bool AreExtensionsSupported(const std::vector<VkExtensionProperties>& allExtensions, std::vector<const char*> wantedExtensions)
		{
			R3_PROF_EVENT();
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

		int ChooseGraphicsPhysicalDevice(const std::vector<PhysicalDeviceDescriptor>& devices, VkSurfaceKHR surface)
		{
			R3_PROF_EVENT();
			for (int i = 0; i < devices.size(); ++i)
			{
				// is it discrete?
				if (devices[i].m_properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
				{
					// does it have a graphics queue and a queue that can present to the surface?
					QueueFamilyIndices qfi = FindQueueFamilyIndices(devices[i], surface);
					bool hasQueueFamilies = qfi.m_graphicsIndex != -1 && qfi.m_presentIndex != -1;
					bool extensionsSupported = AreExtensionsSupported(devices[i].m_supportedExtensions, GetRequiredDeviceExtensions());
					if (hasQueueFamilies && extensionsSupported)
					{
						// does it support a valid swap chain for the surface?
						SwapchainDescriptor swapChainSupport = GetMatchingSwapchains(devices[i].m_device, surface);
						if (swapChainSupport.m_formats.size() > 0 && swapChainSupport.m_presentModes.size() > 0)
						{
							return i;
						}
					}
				}
			}
			return -1;
		}

		QueueFamilyIndices FindQueueFamilyIndices(const PhysicalDeviceDescriptor& pdd, VkSurfaceKHR surface)
		{
			R3_PROF_EVENT();
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

		VkDevice CreateLogicalDevice(const PhysicalDeviceDescriptor& pdd, VkSurfaceKHR surface, bool enablyDynamicRendering, bool enableValidationLayers)
		{
			R3_PROF_EVENT();
			std::vector<VkDeviceQueueCreateInfo> queues;	// which queues (and how many) do we want?

			// create queues (we only need one per unique family index!)
			QueueFamilyIndices qfi = FindQueueFamilyIndices(pdd, surface);
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
			VkPhysicalDeviceFeatures requiredFeatures{};

			// Create the device
			VkDeviceCreateInfo deviceCreate = { 0 };
			deviceCreate.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
			deviceCreate.queueCreateInfoCount = static_cast<uint32_t>(queues.size());
			deviceCreate.pQueueCreateInfos = queues.data();

			// pass the same validation layers
			std::vector<const char*> requiredLayers;
			if(enableValidationLayers)
			{
				LogInfo("Enabling validation layer");
				requiredLayers.push_back("VK_LAYER_KHRONOS_validation");
			}
			deviceCreate.enabledLayerCount = static_cast<uint32_t>(requiredLayers.size());
			deviceCreate.ppEnabledLayerNames = requiredLayers.data();
			std::vector<const char*> extensions = GetRequiredDeviceExtensions();
			deviceCreate.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
			deviceCreate.ppEnabledExtensionNames = extensions.data();
			deviceCreate.pEnabledFeatures = &requiredFeatures;

			// Dynamic rendering is enabled via a feature flag
			VkPhysicalDeviceDynamicRenderingFeatures pddr = { 0 };
			if (enablyDynamicRendering)
			{
				pddr.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES;
				pddr.dynamicRendering = true;
				deviceCreate.pNext = &pddr;
			}

			VkDevice newDevice = nullptr;
			VkResult r = vkCreateDevice(pdd.m_device, &deviceCreate, nullptr, &newDevice);
			CheckResult(r);
			return newDevice;
		}

		SwapchainDescriptor GetMatchingSwapchains(const VkPhysicalDevice& physDevice, VkSurfaceKHR surface)
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
	}
}