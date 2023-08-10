#include "render_system.h"
#include "window.h"
#include "core/file_io.h"
#include "core/profiler.h"
#include "engine/systems/event_system.h"
#include "vulkan_helpers.h"
#include "pipeline_builder.h"
#include <vulkan/vulkan.h>
#include <vulkan/vk_enum_string_helper.h>
#include <vk_mem_alloc.h>
#include <SDL.h>
#include <SDL_events.h>
#include <SDL_vulkan.h>
#include <fmt/format.h>
#include <set>
#include <array>

namespace R3
{
	static constexpr bool c_validationLayersEnabled = true;
	static constexpr int c_maxFramesInFlight = 2;	// let the cpu get ahead of the gpu by this many frames

	struct FrameData
	{
		VkCommandBuffer m_graphicsCmdBuffer;	// graphics queue cmds
		VkSemaphore m_imageAvailableSemaphore;	// signalled when new swap chain image available
		VkSemaphore m_renderFinishedSemaphore;	// signalled when m_graphicsCmdBuffer has been fully submitted to a queue
		VkFence m_inFlightFence;				// signalled when previous cmd buffer finished executing (initialised as signalled)
	};

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

	struct AllocatedBuffer
	{
		VmaAllocation m_allocation = {};
		VkBuffer m_buffer = {};
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
		VkExtent2D m_swapChainExtents;
		VkSurfaceFormatKHR m_swapChainFormat;
		VkPresentModeKHR m_swapChainPresentMode;
		std::vector<VkImage> m_swapChainImages;
		std::vector<VkImageView> m_swapChainImageViews;
		std::vector<VkFramebuffer> m_swapChainFramebuffers; // references the swap chain image views
		VmaAllocator m_allocator;	// vma
		VkCommandPool m_graphicsCommandPool;	// allocates graphics queue command buffers
		FrameData m_perFrameData[c_maxFramesInFlight];	// contains per frame cmd buffers, sync objects
		int m_currentFrame = 0;
		VkFence m_immediateSubmitFence;

		VkRenderPass m_mainRenderPass;
		VkPipeline m_simpleTriPipeline;
		VkPipeline m_simpleTriFromBuffersPipeline;
		AllocatedBuffer m_posColourVertexBuffer;
		VkPipelineLayout m_simplePipelineLayout;	// no descriptors, nada

		// helpers
		FrameData& ThisFrameData()
		{
			return m_perFrameData[m_currentFrame];
		}
	};

	struct PosColourVertex {
		glm::vec2 m_position;
		glm::vec3 m_colour;

		static VkVertexInputBindingDescription GetInputBindingDescription() 
		{
			// we just want to bind a single buffer
			VkVertexInputBindingDescription bindingDesc = {0};
			bindingDesc.binding = 0;
			bindingDesc.stride = sizeof(PosColourVertex);
			bindingDesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
			return bindingDesc;
		}

		static std::array<VkVertexInputAttributeDescription, 2> GetAttributeDescriptions() 
		{
			// 2 attributes, position and colour stored in buffer 0
			std::array<VkVertexInputAttributeDescription, 2> attributes {};
			attributes[0].binding = 0;
			attributes[0].location = 0;	// location visible from shader
			attributes[0].format = VK_FORMAT_R32G32_SFLOAT;	// pos = vec2
			attributes[0].offset = offsetof(PosColourVertex, m_position);
			attributes[1].binding = 0;
			attributes[1].location = 1;	
			attributes[1].format = VK_FORMAT_R32G32B32_SFLOAT;	// colour = vec3
			attributes[1].offset = offsetof(PosColourVertex, m_colour);
			return attributes;
		}
	};

	bool CheckResult(const VkResult& r)
	{
		if (r)
		{
			std::string errorString = string_VkResult(r);
			fmt::print("Vulkan Error! {}\n", errorString);
			int* crashMe = nullptr;
			*crashMe = 1;
		}
		return !r;
	}

	// prefer using this one!
	AllocatedBuffer CreateBuffer(VmaAllocator& allocator, uint64_t sizeBytes, VkBufferUsageFlags bufferUsage, 
		VmaMemoryUsage memUsage = VMA_MEMORY_USAGE_AUTO, uint32_t allocFlags = 0)
	{
		R3_PROF_EVENT();
		AllocatedBuffer newBuffer;
		VkBufferCreateInfo bci = { 0 };
		bci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bci.size = sizeBytes;
		bci.usage = bufferUsage;

		VmaAllocationCreateInfo allocInfo = { 0 };
		allocInfo.usage = memUsage;
		allocInfo.flags = allocFlags;

		VkResult r = vmaCreateBuffer(allocator, &bci, &allocInfo, &newBuffer.m_buffer, &newBuffer.m_allocation, nullptr);
		if (!CheckResult(r))
		{
			fmt::print("Failed to create buffer of size {} bytes", sizeBytes);
		}
		return newBuffer;
	}

	bool RunCommandsImmediate(VkDevice d, VkQueue cmdQueue, VkCommandPool cmdPool, VkFence waitFence, std::function<void(VkCommandBuffer&)> fn)
	{
		// create a temporary cmd buffer from the pool
		VkCommandBufferAllocateInfo allocInfo = {0};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandPool = cmdPool;
		allocInfo.commandBufferCount = 1;

		VkCommandBuffer commandBuffer;
		if (!CheckResult(vkAllocateCommandBuffers(d, &allocInfo, &commandBuffer)))
		{
			fmt::print("Failed to create cmd buffer\n");
			return false;
		}

		VkCommandBufferBeginInfo beginInfo = {0};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;	// we will only submit this buffer once
		if(!CheckResult(vkBeginCommandBuffer(commandBuffer, &beginInfo)))
		{
			fmt::print("Failed to begin cmd buffer\n");
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
		CheckResult(vkWaitForFences(d, 1, &waitFence, true, 9999999999));
		CheckResult(vkResetFences(d, 1, &waitFence));

		// We can free the cmd buffer
		vkFreeCommandBuffers(d, cmdPool, 1, &commandBuffer); 
		
		return true;
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
		R3_PROF_EVENT();
		RegisterTick("Render::DrawFrame", [this]() {
			return DrawFrame();
		});
	}

	bool RenderSystem::DrawFrame()
	{
		R3_PROF_EVENT();

		if (m_isWindowMinimised)
		{
			return true;
		}
		if (m_recreateSwapchain)
		{
			if (!RecreateSwapchainAndFramebuffers())
			{
				fmt::print("Failed to recreate swap chain\n");
				return false;
			}
			m_recreateSwapchain = false;
		}

		auto& fd = m_vk->ThisFrameData();
		{
			R3_PROF_STALL("Wait for fence");
			// wait for the previous frame to finish with infinite timeout (blocking call)
			CheckResult(vkWaitForFences(m_vk->m_device, 1, &fd.m_inFlightFence, VK_TRUE, UINT64_MAX));
		}

		uint32_t swapImageIndex = 0;
		{
			R3_PROF_EVENT("Acquire swap image");
			// acquire the next swap chain image (blocks with infinite timeout)
			// m_imageAvailableSemaphore will be signalled when we are able to draw to the image
			// note that fences can also be passed here
			auto r = vkAcquireNextImageKHR(m_vk->m_device, m_vk->m_swapChain, UINT64_MAX, fd.m_imageAvailableSemaphore, VK_NULL_HANDLE, &swapImageIndex);
			if (r == VK_ERROR_OUT_OF_DATE_KHR)
			{
				m_recreateSwapchain = true;
				return true; // dont continue
			}
			else if (!CheckResult(r))
			{
				fmt::print("Failed to aqcuire next swap chain image index");
				return false;
			}
		}

		// at this point we are definitely going to draw, so reset the inflight fence
		CheckResult(vkResetFences(m_vk->m_device, 1, &fd.m_inFlightFence));

		// reset + record the cmd buffer
		// (its safe since we waited on the inflight fence earlier)
		{
			R3_PROF_EVENT("Reset/Record cmd buffer");
			CheckResult(vkResetCommandBuffer(fd.m_graphicsCmdBuffer, 0));
			RecordCommandBuffer(swapImageIndex);
		}

		// submit the cmd buffer to the graphics queue
		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.pCommandBuffers = &fd.m_graphicsCmdBuffer;	// which buffer to submit
		submitInfo.commandBufferCount = 1;

		// we describe which sync objects to wait on before execution can begin
		// and at which stage in the pipeline we should wait for them
		VkSemaphore waitSemaphores[] = { fd.m_imageAvailableSemaphore };	// wait on the newly aquired image to be available 
		VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };	// wait before we try to write to its colour attachments
		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pWaitSemaphores = waitSemaphores;
		submitInfo.pWaitDstStageMask = waitStages;

		// signal m_renderFinishedSemaphore once execution completes
		VkSemaphore signalSemaphores[] = { fd.m_renderFinishedSemaphore };
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = signalSemaphores;

		{
			R3_PROF_EVENT("Submit");
			// submit the cmd buffer to the queue, the inflight fence will be signalled when execution completes
			if (!CheckResult(vkQueueSubmit(m_vk->m_graphicsQueue, 1, &submitInfo, fd.m_inFlightFence)))
			{
				fmt::print("failed to submit draw command buffer!");
				return false;
			}
		}

		// present!
		// this will put the image we just rendered into the visible window.
		VkPresentInfoKHR presentInfo = {};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		presentInfo.pNext = nullptr;
		presentInfo.pSwapchains = &m_vk->m_swapChain;
		presentInfo.swapchainCount = 1;
		presentInfo.pWaitSemaphores = &fd.m_renderFinishedSemaphore;		// wait on this semaphore before presenting
		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pImageIndices = &swapImageIndex;
		{
			R3_PROF_EVENT("Present");
			auto r = vkQueuePresentKHR(m_vk->m_presentQueue, &presentInfo);
			if (r == VK_ERROR_OUT_OF_DATE_KHR)
			{
				m_recreateSwapchain = true;
			}
			else if (!CheckResult(r))
			{
				fmt::print("Failed to present!");
				return false;
			}
		}

		m_vk->m_currentFrame = (m_vk->m_currentFrame + 1) % c_maxFramesInFlight;

		return true;
	}

	void RenderSystem::OnSystemEvent(void* ev)
	{
		R3_PROF_EVENT();
		auto theEvent = (SDL_Event*)ev;
		if (theEvent->type == SDL_WINDOWEVENT)
		{
			const auto et = theEvent->window.event;
			if (et == SDL_WINDOWEVENT_SIZE_CHANGED || et == SDL_WINDOWEVENT_RESIZED)
			{
				if (m_vk->m_swapChainExtents.width != theEvent->window.data1 || m_vk->m_swapChainExtents.height != theEvent->window.data2)
				{
					m_recreateSwapchain = true;
				}
			}
			if (et == SDL_WINDOWEVENT_MINIMIZED)
			{
				m_isWindowMinimised = true;
			}
			else if (et == SDL_WINDOWEVENT_RESTORED || et == SDL_WINDOWEVENT_MAXIMIZED)
			{
				m_isWindowMinimised = false;
			}
		}
	}

	bool RenderSystem::Init()
	{
		R3_PROF_EVENT();

		auto events = GetSystem<EventSystem>();
		events->RegisterEventHandler([this](void* event) {
			OnSystemEvent(event);
		});

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

		if (!InitialiseVMA())
		{
			fmt::print("Failed to initialise vulkan memory allocator\n");
			return false;
		}

		if (!CreateSwapchain())
		{
			fmt::print("Failed to create swapchain\n");
			return false;
		}

		if (!CreateRenderPass())
		{
			fmt::print("Failed to create render pass\n");
			return false;
		}

		if (!CreateSimpleTriPipelines())
		{
			fmt::print("Failed to create pipelines\n");
			return false;
		}

		if (!CreateFramebuffers())	// must happen after passes created!
		{
			fmt::print("Failed to create frame buffers\n");
			return false;
		}

		if (!CreateCommandPool())
		{
			fmt::print("Failed to create command pool\n");
			return false;
		}

		if (!CreateCommandBuffers())
		{
			fmt::print("Failed to create command buffer\n");
			return false;
		}

		if (!CreateSyncObjects())
		{
			fmt::print("Failed to create sync objects");
			return false;
		}

		if (!CreateMesh())
		{
			fmt::print("Failed to create mesh");
			return false;
		}

		m_mainWindow->Show();

		return true;
	}

	void RenderSystem::Shutdown()
	{
		R3_PROF_EVENT();

		m_mainWindow->Hide();

		// Wait for rendering to finish before shutting down
		CheckResult(vkDeviceWaitIdle(m_vk->m_device));

		// Destroy the mesh buffers
		vmaDestroyBuffer(m_vk->m_allocator, m_vk->m_posColourVertexBuffer.m_buffer, m_vk->m_posColourVertexBuffer.m_allocation);

		// Destroy the sync objects
		vkDestroyFence(m_vk->m_device, m_vk->m_immediateSubmitFence, nullptr);
		for (int f = c_maxFramesInFlight - 1; f >= 0; --f)	// destroy sync objects in reverse order
		{
			vkDestroyFence(m_vk->m_device, m_vk->m_perFrameData[f].m_inFlightFence, nullptr);
			vkDestroySemaphore(m_vk->m_device, m_vk->m_perFrameData[f].m_renderFinishedSemaphore, nullptr);
			vkDestroySemaphore(m_vk->m_device, m_vk->m_perFrameData[f].m_imageAvailableSemaphore, nullptr);
		}
		
		//cmd buffers do not need to be destroyed, removing the pool is enough
		vkDestroyCommandPool(m_vk->m_device, m_vk->m_graphicsCommandPool, nullptr);

		// destroy pipelines + layouts
		vkDestroyPipeline(m_vk->m_device, m_vk->m_simpleTriFromBuffersPipeline, nullptr);
		vkDestroyPipeline(m_vk->m_device, m_vk->m_simpleTriPipeline, nullptr);
		vkDestroyPipelineLayout(m_vk->m_device, m_vk->m_simplePipelineLayout, nullptr);

		// render passes
		vkDestroyRenderPass(m_vk->m_device, m_vk->m_mainRenderPass, nullptr);

		// swap chain, images, framebuffers
		DestroySwapchain();

		vmaDestroyAllocator(m_vk->m_allocator);

		vkDestroyDevice(m_vk->m_device, nullptr);
		vkDestroySurfaceKHR(m_vk->m_vkInstance, m_vk->m_mainSurface, nullptr);
		vkDestroyInstance(m_vk->m_vkInstance, nullptr);
		m_mainWindow = nullptr;
	}

	bool RenderSystem::CreateMesh()
	{
		R3_PROF_EVENT();

		PosColourVertex verts[3];
		verts[0].m_position = { 0.5f, 0.0f };
		verts[1].m_position = { 0.0f, -1.0f };
		verts[2].m_position = { 1.0f,  -1.0f };
		verts[0].m_colour = { 0.f, 0.f, 1.0f };
		verts[1].m_colour = { 0.f, 1.f, 0.5f };
		verts[2].m_colour = { 1.f, 0.f, 0.1f };

		AllocatedBuffer stagingBuffer = CreateBuffer(m_vk->m_allocator, 
			sizeof(PosColourVertex) * 3,
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT, 
			VMA_MEMORY_USAGE_AUTO, 
			VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
		);
		m_vk->m_posColourVertexBuffer = CreateBuffer(m_vk->m_allocator,	
			sizeof(PosColourVertex) * 3,
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT
		);
		if (m_vk->m_posColourVertexBuffer.m_buffer == VK_NULL_HANDLE || stagingBuffer.m_buffer == VK_NULL_HANDLE)
		{
			fmt::print("Failed to create buffers");
			return false;
		}

		// upload the data to the staging buffer
		void* mappedData = nullptr;
		if (CheckResult(vmaMapMemory(m_vk->m_allocator, stagingBuffer.m_allocation, &mappedData)))
		{
			memcpy(mappedData, verts, 3 * sizeof(PosColourVertex));
			vmaUnmapMemory(m_vk->m_allocator, stagingBuffer.m_allocation);
		}

		// copy from staging to vertex buffer using immediate cmd submit
		RunCommandsImmediate(m_vk->m_device, m_vk->m_graphicsQueue, m_vk->m_graphicsCommandPool, m_vk->m_immediateSubmitFence,
			[&](VkCommandBuffer& buf)
		{
			VkBufferCopy copyRegion{};
			copyRegion.srcOffset = 0;
			copyRegion.dstOffset = 0;
			copyRegion.size = sizeof(PosColourVertex) * 3;
			vkCmdCopyBuffer(buf, stagingBuffer.m_buffer, m_vk->m_posColourVertexBuffer.m_buffer, 1, &copyRegion);
		});

		// done with the staging buffer
		vmaDestroyBuffer(m_vk->m_allocator, stagingBuffer.m_buffer, stagingBuffer.m_allocation);

		return m_vk->m_posColourVertexBuffer.m_buffer != VK_NULL_HANDLE;
	}

	bool RenderSystem::InitialiseVMA()
	{
		R3_PROF_EVENT();
		//initialize the memory allocator
		VmaAllocatorCreateInfo allocatorInfo = {0};
		allocatorInfo.physicalDevice = m_vk->m_physicalDevice.m_device;
		allocatorInfo.device = m_vk->m_device;
		allocatorInfo.instance = m_vk->m_vkInstance;
		return CheckResult(vmaCreateAllocator(&allocatorInfo, &m_vk->m_allocator));
	}

	void RenderSystem::DestroySwapchain()
	{
		R3_PROF_EVENT();
		for (auto framebuffer : m_vk->m_swapChainFramebuffers) {
			vkDestroyFramebuffer(m_vk->m_device, framebuffer, nullptr);
		}

		for (auto imageView : m_vk->m_swapChainImageViews) {
			vkDestroyImageView(m_vk->m_device, imageView, nullptr);
		}

		m_vk->m_swapChainImageViews.clear();
		m_vk->m_swapChainImages.clear();
		vkDestroySwapchainKHR(m_vk->m_device, m_vk->m_swapChain, nullptr);
	}

	bool RenderSystem::RecreateSwapchainAndFramebuffers()
	{
		R3_PROF_EVENT();
		CheckResult(vkDeviceWaitIdle(m_vk->m_device));

		DestroySwapchain();

		if (!CreateSwapchain())
		{
			fmt::print("Failed to create swapchain\n");
			return false;
		}

		if (!CreateFramebuffers())
		{
			fmt::print("Failed to create frame buffers\n");
			return false;
		}

		return true;
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

	std::vector<const char*> GetSDLRequiredInstanceExtensions(SDL_Window* w) 
	{
		R3_PROF_EVENT();
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

	std::vector<PhysicalDeviceDescriptor> GetAllPhysicalDevices(VkInstance& instance)
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

	// find indices for the queue families we care about
	QueueFamilyIndices FindQueueFamilyIndices(const PhysicalDeviceDescriptor& pdd, VkSurfaceKHR surface = VK_NULL_HANDLE)
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

	bool RenderSystem::CreateSyncObjects()
	{
		R3_PROF_EVENT();
		// semaphores + fences dont have many params
		VkSemaphoreCreateInfo semaphoreInfo = {0};
		semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
		VkFenceCreateInfo fenceInfo = {0};
		fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;	// create the fence signalled since we wait on it immediately
		for (int f = 0; f < c_maxFramesInFlight; ++f)
		{
			FrameData& fd = m_vk->m_perFrameData[f];
			if (!CheckResult(vkCreateSemaphore(m_vk->m_device, &semaphoreInfo, nullptr, &fd.m_imageAvailableSemaphore)))
			{
				fmt::print("Failed to create semaphore");
				return false;
			}
			if (!CheckResult(vkCreateSemaphore(m_vk->m_device, &semaphoreInfo, nullptr, &fd.m_renderFinishedSemaphore)))
			{
				fmt::print("Failed to create semaphore");
				return false;
			}
			if (!CheckResult(vkCreateFence(m_vk->m_device, &fenceInfo, nullptr, &fd.m_inFlightFence)))
			{
				fmt::print("Failed to create fence");
				return false;
			}
		}

		VkFenceCreateInfo fenceInfoNotSignalled = fenceInfo;
		fenceInfoNotSignalled.flags = 0;
		if (!CheckResult(vkCreateFence(m_vk->m_device, &fenceInfoNotSignalled, nullptr, &m_vk->m_immediateSubmitFence)))
		{
			fmt::print("Failed to create immediate submit fence");
			return false;
		}

		return true;
	}

	bool RenderSystem::RecordCommandBuffer(int swapImageIndex)
	{
		R3_PROF_EVENT();

		auto& cmdBuffer = m_vk->ThisFrameData().m_graphicsCmdBuffer;

		// pipeline dynamic state
		VkViewport viewport = { 0 };
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = (float)m_vk->m_swapChainExtents.width;
		viewport.height = (float)m_vk->m_swapChainExtents.height;
		viewport.minDepth = 0.0f;	// normalised! must be between 0 and 1
		viewport.maxDepth = 1.0f;	// ^^
		VkRect2D scissor = { 0 };
		scissor.offset = { 0, 0 };
		scissor.extent = m_vk->m_swapChainExtents;	// draw the full image

		// start writing
		VkCommandBufferBeginInfo beginInfo = {0};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		if (!CheckResult(vkBeginCommandBuffer(cmdBuffer, &beginInfo)))	// resets the cmd buffer
		{
			fmt::print("failed to begin recording command buffer!");
			return false;
		}

		// start the render pass 
		VkRenderPassBeginInfo renderPassInfo = {0};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassInfo.renderPass = m_vk->m_mainRenderPass;
		renderPassInfo.framebuffer = m_vk->m_swapChainFramebuffers[swapImageIndex];
		renderPassInfo.renderArea.offset = { 0, 0 };			// offset/extents to draw to
		renderPassInfo.renderArea.extent = m_vk->m_swapChainExtents;
		// attachment clear op values -  clear colour/depth values go here
		VkClearValue clearColor = { {{0.0f, 0.0f, 0.0f, 1.0f}} };
		renderPassInfo.clearValueCount = 1;
		renderPassInfo.pClearValues = &clearColor;
		// VK_SUBPASS_CONTENTS_INLINE - commands are all stored in primary buffer
		vkCmdBeginRenderPass(cmdBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

		// bind the pipeline
		vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_vk->m_simpleTriPipeline);

		// Set pipeline dynamic state
		vkCmdSetViewport(cmdBuffer, 0, 1, &viewport);
		vkCmdSetScissor(cmdBuffer, 0, 1, &scissor);

		// draw one triangle made of 3 verts
		vkCmdDraw(cmdBuffer, 3, 1, 0, 0);

		// bind the 2nd pipeline
		vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_vk->m_simpleTriFromBuffersPipeline);

		// Set pipeline dynamic state
		vkCmdSetViewport(cmdBuffer, 0, 1, &viewport);
		vkCmdSetScissor(cmdBuffer, 0, 1, &scissor);

		// bind the vertex buffer with offset 0
		VkDeviceSize offset = 0;
		vkCmdBindVertexBuffers(cmdBuffer, 0, 1, &m_vk->m_posColourVertexBuffer.m_buffer, &offset);

		// draw one triangle made of 3 verts
		vkCmdDraw(cmdBuffer, 3, 1, 0, 0);

		// end render pass
		vkCmdEndRenderPass(cmdBuffer);

		if (!CheckResult(vkEndCommandBuffer(cmdBuffer)))
		{
			fmt::print("failed to end recording command buffer!\n");
			return false;
		}

		return true;
	}

	bool RenderSystem::CreateCommandBuffers()
	{
		R3_PROF_EVENT();
		// allocate the graphics cmd buffer from the graphics cmd pool
		VkCommandBufferAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.commandPool = m_vk->m_graphicsCommandPool;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;	// primary = can be submitted to queue, can't be called from other buffers
															// secondary = can't be directly submitted, can be called from other primary buffers
		allocInfo.commandBufferCount = c_maxFramesInFlight;
		std::vector<VkCommandBuffer> cmdBuffers(c_maxFramesInFlight);
		if (!CheckResult(vkAllocateCommandBuffers(m_vk->m_device, &allocInfo, cmdBuffers.data())))
		{
			fmt::print("failed to allocate command buffers!\n");
			return false;
		}

		for (int f = 0; f < c_maxFramesInFlight; ++f)
		{
			m_vk->m_perFrameData[f].m_graphicsCmdBuffer = cmdBuffers[f];
		}

		return true;
	}

	bool RenderSystem::CreateCommandPool()
	{
		R3_PROF_EVENT();
		// find a graphics queue
		QueueFamilyIndices queueFamilyIndices = FindQueueFamilyIndices(m_vk->m_physicalDevice, m_vk->m_mainSurface);

		// create
		VkCommandPoolCreateInfo poolInfo = {};
		poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;	// - allow individual buffers to be re-recorded
																			// // VK_COMMAND_POOL_CREATE_TRANSIENT_BIT - hint that buffers are re-recorded often
		poolInfo.queueFamilyIndex = queueFamilyIndices.m_graphicsIndex;		// graphics queue pls

		if (!CheckResult(vkCreateCommandPool(m_vk->m_device, &poolInfo, nullptr, &m_vk->m_graphicsCommandPool)))
		{
			fmt::print("failed to create command pool!");
			return false;
		}

		return true;
	}

	bool RenderSystem::CreateFramebuffers()
	{
		R3_PROF_EVENT();
		// create a frame buffer for each swap chain image
		m_vk->m_swapChainFramebuffers.resize(m_vk->m_swapChainImageViews.size());
		for (int i = 0; i < m_vk->m_swapChainImageViews.size(); ++i)
		{
			VkImageView attachments[] = 
			{
				m_vk->m_swapChainImageViews[i]
			};

			VkFramebufferCreateInfo framebufferInfo{};
			framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			framebufferInfo.renderPass = m_vk->m_mainRenderPass;	// frame buffer must be compatible with a pass
			framebufferInfo.attachmentCount = 1;
			framebufferInfo.pAttachments = attachments;
			framebufferInfo.width = m_vk->m_swapChainExtents.width;
			framebufferInfo.height = m_vk->m_swapChainExtents.height;
			framebufferInfo.layers = 1;	// no arrays pls

			if (!CheckResult(vkCreateFramebuffer(m_vk->m_device, &framebufferInfo, nullptr, &m_vk->m_swapChainFramebuffers[i]))) 
			{
				fmt::print("failed to create framebuffer!");
				return false;
			}
		}

		return true;
	}

	bool RenderSystem::CreateRenderPass()
	{
		R3_PROF_EVENT();
		// Describe all image attachments for the render pass

		// colour attachment from swap chain
		VkAttachmentDescription colourDesc = {};
		colourDesc.format = m_vk->m_swapChainFormat.format;
		colourDesc.samples = VK_SAMPLE_COUNT_1_BIT;			// no msaa
		colourDesc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;	// clear the contents of the image before this pass
		colourDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;	// store the contents after this pass
		colourDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;		// we dont use stencil
		colourDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;	// ^^
		colourDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;			// we dont care about initial layout
		colourDesc.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;		// layout required for swap chain images

		// Each sub-pass can reference the attachments above with different layouts
		VkAttachmentReference colorAttachmentRef = {};
		colorAttachmentRef.attachment = 0;	// colour attachment
		colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;	// we want the image to be optimal for rendering
																				// note vulkan will handle this transition for you between sub-passes

		// Setup our single subpass
		VkSubpassDescription subpassDesc = {};
		subpassDesc.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;	// this is a graphics subpass
		subpassDesc.colorAttachmentCount = 1;	// note the index of the attachments in this array match the layout(location = x) out vec4 gl_colour in the shader!
		subpassDesc.pColorAttachments = &colorAttachmentRef;

		// create the full pass!
		VkRenderPassCreateInfo rpci = {};
		rpci.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		rpci.attachmentCount = 1;
		rpci.pAttachments = &colourDesc;
		rpci.subpassCount = 1;
		rpci.pSubpasses = &subpassDesc;
		if (!CheckResult(vkCreateRenderPass(m_vk->m_device, &rpci, nullptr, &m_vk->m_mainRenderPass))) 
		{
			fmt::print("failed to create render pass!");
			return false;
		}

		return true;
	}

	bool RenderSystem::CreateSimpleTriPipelines()
	{
		R3_PROF_EVENT();

		// Load the shaders
		// Note the shader modules can be destroyed once the pipeline is created
		// compiled shader state is associated with the pipeline, not the module!
		std::string basePath = "shaders_spirv\\vk_tutorials\\";
		auto singleTriVertexShader = VulkanHelpers::LoadShaderModule(m_vk->m_device, basePath + "fixed_triangle.vert.spv");
		auto singleTriFragmentShader = VulkanHelpers::LoadShaderModule(m_vk->m_device, basePath + "fixed_triangle.frag.spv");
		if (singleTriVertexShader == VK_NULL_HANDLE || singleTriFragmentShader == VK_NULL_HANDLE)
		{
			fmt::print("Failed to create shader modules");
			return false;
		}

		PipelineBuilder pb;

		// describe the stages and which shader is used
		pb.m_shaderStages.push_back(VulkanHelpers::CreatePipelineShaderState(VK_SHADER_STAGE_VERTEX_BIT, singleTriVertexShader));
		pb.m_shaderStages.push_back(VulkanHelpers::CreatePipelineShaderState(VK_SHADER_STAGE_FRAGMENT_BIT, singleTriFragmentShader));

		// dynamic state must be set each time the pipeline is bound!
		std::vector<VkDynamicState> dynamicStates = {
			VK_DYNAMIC_STATE_VIEWPORT,		// we will set viewport at draw time (lets us handle window resize without recreating pipelines)
			VK_DYNAMIC_STATE_SCISSOR		// same for the scissor data
		};
		pb.m_dynamicState = VulkanHelpers::CreatePipelineDynamicState(dynamicStates);

		// Set up vertex buffer input state. Since we have no input buffers, just set count to 0
		VkPipelineVertexInputStateCreateInfo vertexInputInfo = {0};
		vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		vertexInputInfo.vertexBindingDescriptionCount = 0;
		vertexInputInfo.vertexAttributeDescriptionCount = 0;
		pb.m_vertexInputState = vertexInputInfo;

		// Input assembly describes type of geometry (lines/tris) and topology(strips,lists,etc) to draw
		pb.m_inputAssemblyState = VulkanHelpers::CreatePipelineInputAssemblyState(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);

		// create the viewport state for the pipeline
		// if using dynamic we only need to set the counts
		VkPipelineViewportStateCreateInfo viewportState = {0};
		viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewportState.viewportCount = 1;
		viewportState.scissorCount = 1;
		pb.m_viewportState = viewportState;

		// Setup rasteriser state
		pb.m_rasterState = VulkanHelpers::CreatePipelineRasterState(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_CLOCKWISE);

		// Multisampling state
		pb.m_multisamplingState = VulkanHelpers::CreatePipelineMultiSampleState_SingleSample();

		// Depth-stencil state (we have no depth/stencil right now, disable it all)
		VkPipelineDepthStencilStateCreateInfo depthStencilState = { 0 };
		depthStencilState.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		depthStencilState.depthTestEnable = VK_FALSE;
		depthStencilState.depthWriteEnable = VK_FALSE;
		depthStencilState.depthBoundsTestEnable = VK_FALSE;
		depthStencilState.stencilTestEnable = VK_FALSE;
		pb.m_depthStencilState = depthStencilState;

		// Colour blending state
		// blending state per attachment
		VkPipelineColorBlendAttachmentState colourBlendAttachment = VulkanHelpers::CreatePipelineColourBlendAttachment_NoBlending();	
		std::vector<VkPipelineColorBlendAttachmentState> allAttachments = {
			colourBlendAttachment
		};
		pb.m_colourBlendState = VulkanHelpers::CreatePipelineColourBlendState(allAttachments);	// Pipeline also has some global blending state (constants, logical ops enable)

		// Create an empty (for now) pipeline layout to be used later 
		VkPipelineLayoutCreateInfo pipelineLayoutInfo = {0};
		pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutInfo.setLayoutCount = 0;
		pipelineLayoutInfo.pushConstantRangeCount = 0;
		if (!CheckResult(vkCreatePipelineLayout(m_vk->m_device, &pipelineLayoutInfo, nullptr, &m_vk->m_simplePipelineLayout)))
		{
			fmt::print("Failed to create pipeline layout!\n");
			return false;
		}

		// build the pipeline
		m_vk->m_simpleTriPipeline = pb.Build(m_vk->m_device, m_vk->m_simplePipelineLayout, m_vk->m_mainRenderPass, 0);
		if (m_vk->m_simpleTriPipeline == VK_NULL_HANDLE)
		{
			fmt::print("Failed to create pipeline!\n");
			return false;
		}

		// dont need the shader modules any more!
		vkDestroyShaderModule(m_vk->m_device, singleTriFragmentShader, nullptr);
		vkDestroyShaderModule(m_vk->m_device, singleTriVertexShader, nullptr);

		// build a similar pipeline but with shaders that take vertex data from buffers
		// also passes buffer descriptors!
		pb.m_shaderStages.clear();
		VkShaderModule triBufferVertShader = VulkanHelpers::LoadShaderModule(m_vk->m_device, basePath + "triangle_from_buffers.vert.spv");
		VkShaderModule triBufferFragShader = VulkanHelpers::LoadShaderModule(m_vk->m_device, basePath + "triangle_from_buffers.frag.spv");
		if (triBufferVertShader == VK_NULL_HANDLE || triBufferFragShader == VK_NULL_HANDLE)
		{
			fmt::print("Failed to load fancier shaders\n");
			return false;
		}
		pb.m_shaderStages.push_back(VulkanHelpers::CreatePipelineShaderState(VK_SHADER_STAGE_VERTEX_BIT, triBufferVertShader));
		pb.m_shaderStages.push_back(VulkanHelpers::CreatePipelineShaderState(VK_SHADER_STAGE_FRAGMENT_BIT, triBufferFragShader));

		// bind the buffers + input attrib descriptors
		auto bufferBindingDescriptions = PosColourVertex::GetInputBindingDescription();
		auto attributeDescriptions = PosColourVertex::GetAttributeDescriptions();
		pb.m_vertexInputState.vertexBindingDescriptionCount = 1;
		pb.m_vertexInputState.pVertexBindingDescriptions = &bufferBindingDescriptions;
		pb.m_vertexInputState.vertexAttributeDescriptionCount = attributeDescriptions.size();
		pb.m_vertexInputState.pVertexAttributeDescriptions = attributeDescriptions.data();

		m_vk->m_simpleTriFromBuffersPipeline = pb.Build(m_vk->m_device, m_vk->m_simplePipelineLayout, m_vk->m_mainRenderPass, 0);
		if (m_vk->m_simpleTriFromBuffersPipeline == VK_NULL_HANDLE)
		{
			fmt::print("Failed to create fancier pipeline!\n");
			return false;
		}

		vkDestroyShaderModule(m_vk->m_device, triBufferFragShader, nullptr);
		vkDestroyShaderModule(m_vk->m_device, triBufferVertShader, nullptr);

		return true;
	}

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
			fmt::print("Failed to find a suitable surface format");
			return {};
		}
	}

	VkPresentModeKHR GetSwapchainSurfacePresentMode(const SwapchainDescriptor& sd)
	{
		R3_PROF_EVENT();
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
		R3_PROF_EVENT();
		VkExtent2D extents;
		int w = 0, h = 0;
		SDL_Vulkan_GetDrawableSize(window.GetHandle(), &w, &h);
		extents.width = glm::clamp((uint32_t)w, sd.m_caps.minImageExtent.width, sd.m_caps.maxImageExtent.width);
		extents.height = glm::clamp((uint32_t)h, sd.m_caps.minImageExtent.height, sd.m_caps.maxImageExtent.height);
		return extents;
	}

	bool RenderSystem::CreateSwapchain()
	{
		R3_PROF_EVENT();
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

		// Get all the images created as part of the swapchain
		uint32_t actualImageCount = 0;
		CheckResult(vkGetSwapchainImagesKHR(m_vk->m_device, m_vk->m_swapChain, &actualImageCount, nullptr));
		m_vk->m_swapChainImages.resize(actualImageCount);
		CheckResult(vkGetSwapchainImagesKHR(m_vk->m_device, m_vk->m_swapChain, &actualImageCount, m_vk->m_swapChainImages.data()));
		m_vk->m_swapChainExtents = extents;
		m_vk->m_swapChainFormat = bestFormat;
		m_vk->m_swapChainPresentMode = bestPresentMode;

		// Create image views for every image in the swap chain
		m_vk->m_swapChainImageViews.resize(actualImageCount);
		for (int i = 0; i < actualImageCount; ++i)
		{
			VkImageViewCreateInfo createView = { 0 };
			createView.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			createView.image = m_vk->m_swapChainImages[i];
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
			VkResult r = vkCreateImageView(m_vk->m_device, &createView, nullptr, &m_vk->m_swapChainImageViews[i]);
			if (!CheckResult(r))
			{
				fmt::print("Failed to create image view for swap image {}\n", i);
				return false;
			}
		}

		return true;
	}
	
	bool RenderSystem::CreateWindow()
	{
		R3_PROF_EVENT();
		Window::Properties windowProps;
		windowProps.m_sizeX = 1280;
		windowProps.m_sizeY = 720;
		windowProps.m_title = "R3";
		windowProps.m_flags = Window::CreateResizable;
		m_mainWindow = std::make_unique<Window>(windowProps);
		return m_mainWindow != nullptr;
	}

	bool RenderSystem::CreateSurface()
	{
		R3_PROF_EVENT();
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
		R3_PROF_EVENT();
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
		R3_PROF_EVENT();
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
		R3_PROF_EVENT();
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
		R3_PROF_EVENT();
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