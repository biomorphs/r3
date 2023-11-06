#include "render_system.h"
#include "window.h"
#include "device.h"
#include "swap_chain.h"
#include "core/file_io.h"
#include "core/profiler.h"
#include "core/log.h"
#include "engine/systems/event_system.h"
#include "engine/systems/time_system.h"
#include "engine/systems/imgui_system.h"
#include "entities/systems/entity_system.h"
#include "entities/queries.h"
#include "engine/components/environment_settings.h"
#include "engine/loaded_model.h"
#include "vulkan_helpers.h"
#include "pipeline_builder.h"
#include <vulkan/vulkan.h>
#include <vulkan/vk_enum_string_helper.h>
#include <vk_mem_alloc.h>
#include <imgui_impl_vulkan.h>
#include <imgui_impl_sdl2.h>
#include <SDL.h>
#include <SDL_events.h>
#include <SDL_vulkan.h>
#include <set>
#include <array>

namespace R3
{
	static constexpr int c_maxFramesInFlight = 2;	// let the cpu get ahead of the gpu by this many frames
	using VulkanHelpers::CheckResult;				// laziness
	using VulkanHelpers::FindQueueFamilyIndices;

	struct FrameData
	{
		VkCommandPool m_graphicsCommandPool = VK_NULL_HANDLE;	// allocates graphics queue command buffers
		VkCommandBuffer m_graphicsCmdBuffer = VK_NULL_HANDLE;	// graphics queue cmds
		VkSemaphore m_imageAvailableSemaphore = VK_NULL_HANDLE;	// signalled when new swap chain image available
		VkSemaphore m_renderFinishedSemaphore = VK_NULL_HANDLE;	// signalled when m_graphicsCmdBuffer has been fully submitted to a queue
		VkFence m_inFlightFence = VK_NULL_HANDLE;				// signalled when previous cmd buffer finished executing (initialised as signalled)
	};

	struct AllocatedImage
	{
		VkImage m_image;
		VmaAllocation m_allocation;
	};

	struct ImGuiVkStuff
	{
		VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;
	};

	struct RenderSystem::VkStuff
	{
		AllocatedImage m_depthBufferImage;	// note the tutorial may be wrong here, we might want one per swap chain image!
		VkImageView m_depthBufferView;
		VkFormat m_depthBufferFormat;
		FrameData m_perFrameData[c_maxFramesInFlight];	// contains per frame cmd buffers, sync objects
		int m_currentFrame = 0;
		VkFence m_immediateSubmitFence = VK_NULL_HANDLE;

		VkPipeline m_simpleTriPipeline = VK_NULL_HANDLE;
		VkPipeline m_simpleTriFromBuffersPipeline = VK_NULL_HANDLE;
		VkPipeline m_simpleTriFromBuffersPushConstantPipeline = VK_NULL_HANDLE;
		AllocatedBuffer m_posColourVertexBuffer;
		VkPipelineLayout m_simplePipelineLayout = VK_NULL_HANDLE;	// no descriptors, nada
		VkPipelineLayout m_simpleLayoutWithPushConstant = VK_NULL_HANDLE;

		ImGuiVkStuff m_imgui;

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
			VkVertexInputBindingDescription bindingDesc = { 0 };
			bindingDesc.binding = 0;
			bindingDesc.stride = sizeof(PosColourVertex);
			bindingDesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
			return bindingDesc;
		}

		static std::array<VkVertexInputAttributeDescription, 2> GetAttributeDescriptions()
		{
			// 2 attributes, position and colour stored in buffer 0
			std::array<VkVertexInputAttributeDescription, 2> attributes{};
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

	struct PushConstant {
		glm::vec4 m_colour;
		glm::mat4 m_transform;
	};

	RenderSystem::RenderSystem()
	{
		m_vk = std::make_unique<VkStuff>();
	}

	RenderSystem::~RenderSystem()
	{
		m_vk = nullptr;
	}

	glm::vec2 RenderSystem::GetWindowExtents()
	{
		auto e = m_swapChain->GetExtents();
		return { e.width, e.height };
	}

	void RenderSystem::RegisterTickFns()
	{
		R3_PROF_EVENT();
		RegisterTick("Render::DrawFrame", [this]() {
			return DrawFrame();
		});
		RegisterTick("Render::ShowGui", [this]() {
			return ShowGui();
		});
	}

	void RenderSystem::ProcessEnvironmentSettings()
	{
		R3_PROF_EVENT();
		auto entities = GetSystem<Entities::EntitySystem>();
		auto w = entities->GetActiveWorld();
		if (w)
		{
			auto getClearColour = [this](const Entities::EntityHandle& e, EnvironmentSettingsComponent& cmp) {
				m_mainPassClearColour = cmp.m_clearColour;
				return true;
			};
			Entities::Queries::ForEach<EnvironmentSettingsComponent>(w, getClearColour);
		}
	}

	bool RenderSystem::ShowGui()
	{
		R3_PROF_EVENT();
		ImGui::Begin("Render System", nullptr, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize);
		{
			auto str = std::format("Swap chain extents: {}x{}", m_swapChain->GetExtents().width, m_swapChain->GetExtents().height);
			ImGui::Text(str.c_str());
			str = std::format("Swap chain images: {}", m_swapChain->GetImages().size());
			ImGui::Text(str.c_str());
			auto timeSys = GetSystem<TimeSystem>();
			str = std::format("FPS/Frame Time: {:.1f} / {:.4f}ms", 1.0 / timeSys->GetVariableDeltaTime(), timeSys->GetVariableDeltaTime());
			ImGui::Text(str.c_str());
		}
		ImGui::End();
		return true;
	}

	bool RenderSystem::CreateSwapchain()
	{
		m_swapChain = std::make_unique<Swapchain>();
		return m_swapChain->Initialise(*m_device, *m_mainWindow);
	}

	bool RenderSystem::PrepareSwapchain()
	{
		if (m_isWindowMinimised)
		{
			return false;
		}
		if (m_recreateSwapchain)
		{
			if (!RecreateSwapchain())
			{
				LogError("Failed to recreate swap chain");
				return false;
			}
			m_recreateSwapchain = false;
		}
		return true;
	}

	bool RenderSystem::DrawFrame()
	{
		R3_PROF_EVENT();

		ProcessEnvironmentSettings();

		// Always 'render' the ImGui frame so we can begin the next one, even if we wont actually draw anything
		ImGui::Render();	

		// Nothing to draw this frame if this returns false
		if (!PrepareSwapchain())
		{
			return true;
		}

		auto& fd = m_vk->ThisFrameData();
		{
			R3_PROF_STALL("Wait for previous frame fence");
			// wait for the previous frame to finish with infinite timeout (blocking call)
			CheckResult(vkWaitForFences(m_device->GetVkDevice(), 1, &fd.m_inFlightFence, VK_TRUE, UINT64_MAX));
		}

		uint32_t swapImageIndex = 0;
		{
			R3_PROF_EVENT("Acquire swap image");
			// acquire the next swap chain image (blocks with infinite timeout)
			// m_imageAvailableSemaphore will be signalled when we are able to draw to the image
			// note that fences can also be passed here
			auto r = vkAcquireNextImageKHR(m_device->GetVkDevice(), m_swapChain->GetSwapchain(), UINT64_MAX, fd.m_imageAvailableSemaphore, VK_NULL_HANDLE, &swapImageIndex);
			if (r == VK_ERROR_OUT_OF_DATE_KHR)
			{
				m_recreateSwapchain = true;
				return true; // dont continue
			}
			else if (!CheckResult(r))
			{
				LogError("Failed to aqcuire next swap chain image index");
				return false;
			}
		}

		// at this point we are definitely going to draw, so reset the inflight fence
		CheckResult(vkResetFences(m_device->GetVkDevice(), 1, &fd.m_inFlightFence));

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
			if (!CheckResult(vkQueueSubmit(m_device->GetGraphicsQueue(), 1, &submitInfo, fd.m_inFlightFence)))
			{
				LogError("failed to submit draw command buffer!");
				return false;
			}
		}

		// present!
		// this will put the image we just rendered into the visible window.
		VkPresentInfoKHR presentInfo = {};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		presentInfo.pNext = nullptr;
		presentInfo.pSwapchains = &m_swapChain->GetSwapchain();
		presentInfo.swapchainCount = 1;
		presentInfo.pWaitSemaphores = &fd.m_renderFinishedSemaphore;		// wait on this semaphore before presenting
		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pImageIndices = &swapImageIndex;
		{
			R3_PROF_EVENT("Present");
			auto r = vkQueuePresentKHR(m_device->GetPresentQueue(), &presentInfo);
			if (r == VK_ERROR_OUT_OF_DATE_KHR)
			{
				m_recreateSwapchain = true;
			}
			else if (!CheckResult(r))
			{
				LogError("Failed to present!");
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
				if (m_swapChain->GetExtents().width != theEvent->window.data1 || m_swapChain->GetExtents().height != theEvent->window.data2)
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

	bool RenderSystem::CreateDevice()
	{
		m_device = std::make_unique<Device>(m_mainWindow.get());
		return m_device->Initialise(true);
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
			LogError("Failed to create window... {}", SDL_GetError());
			return false;
		}

		if (!CreateDevice())
		{
			LogError("Failed to create device!");
			return false;
		}

		if (!CreateSwapchain())
		{
			LogError("Failed to create swap chain");
			return false;
		}

		if (!CreateDepthBuffer())
		{
			LogError("Failed to create depth buffer");
			return false;
		}

		if (!CreateSimpleTriPipelines())
		{
			LogError("Failed to create pipelines");
			return false;
		}

		if (!CreateCommandPools())
		{
			LogError("Failed to create command pools");
			return false;
		}

		if (!CreateCommandBuffers())
		{
			LogError("Failed to create command buffers");
			return false;
		}

		if (!CreateSyncObjects())
		{
			LogError("Failed to create sync objects");
			return false;
		}

		if (!CreateMesh())
		{
			LogError("Failed to create mesh");
			return false;
		}

		LoadedModel cubeMesh;
		if (!R3::LoadModel("models/cube.fbx", cubeMesh))
		{
			LogError("Failed to load cube mesh");
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
		CheckResult(vkDeviceWaitIdle(m_device->GetVkDevice()));

		// Clean up ImGui
		vkDestroyDescriptorPool(m_device->GetVkDevice(), m_vk->m_imgui.m_descriptorPool, nullptr);
		ImGui_ImplVulkan_Shutdown();

		// Destroy the mesh buffers
		vmaDestroyBuffer(m_device->GetVMA(), m_vk->m_posColourVertexBuffer.m_buffer, m_vk->m_posColourVertexBuffer.m_allocation);

		// Destroy the sync objects
		vkDestroyFence(m_device->GetVkDevice(), m_vk->m_immediateSubmitFence, nullptr);
		for (int f = c_maxFramesInFlight - 1; f >= 0; --f)	// destroy sync objects in reverse order
		{
			vkDestroyFence(m_device->GetVkDevice(), m_vk->m_perFrameData[f].m_inFlightFence, nullptr);
			vkDestroySemaphore(m_device->GetVkDevice(), m_vk->m_perFrameData[f].m_renderFinishedSemaphore, nullptr);
			vkDestroySemaphore(m_device->GetVkDevice(), m_vk->m_perFrameData[f].m_imageAvailableSemaphore, nullptr);
		}

		//cmd buffers do not need to be destroyed, removing the pool is enough
		for (int f = c_maxFramesInFlight - 1; f >= 0; --f)	// destroy sync objects in reverse order
		{
			vkDestroyCommandPool(m_device->GetVkDevice(), m_vk->m_perFrameData[f].m_graphicsCommandPool, nullptr);
		}

		// destroy pipelines + layouts
		vkDestroyPipelineLayout(m_device->GetVkDevice(), m_vk->m_simpleLayoutWithPushConstant, nullptr);
		vkDestroyPipelineLayout(m_device->GetVkDevice(), m_vk->m_simplePipelineLayout, nullptr);
		vkDestroyPipeline(m_device->GetVkDevice(), m_vk->m_simpleTriFromBuffersPipeline, nullptr);
		vkDestroyPipeline(m_device->GetVkDevice(), m_vk->m_simpleTriFromBuffersPushConstantPipeline, nullptr);
		vkDestroyPipeline(m_device->GetVkDevice(), m_vk->m_simpleTriPipeline, nullptr);
		
		// clean up depth buffer
		vkDestroyImageView(m_device->GetVkDevice(), m_vk->m_depthBufferView, nullptr);
		vmaDestroyImage(m_device->GetVMA(), m_vk->m_depthBufferImage.m_image, m_vk->m_depthBufferImage.m_allocation);

		m_swapChain->Destroy(*m_device);
		m_swapChain = nullptr;

		m_device = nullptr;
		m_mainWindow = nullptr;
	}

	void RenderSystem::ImGuiNewFrame()
	{
		R3_PROF_EVENT();
		// does order matter? can ImgGui::NewFrame go in imgui system instead?
		ImGui_ImplVulkan_NewFrame();
		ImGui_ImplSDL2_NewFrame(m_mainWindow->GetHandle());
		ImGui::NewFrame();
	}

	bool RenderSystem::InitImGui()
	{
		R3_PROF_EVENT();
		// create a descriptor pool for imgui
		VkDescriptorPoolSize pool_sizes[] =		// this is way bigger than it needs to be!
		{
			{ VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
			{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
			{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
			{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
		};
		VkDescriptorPoolCreateInfo pool_info = {};
		pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;	// allows us to free individual descriptor sets
		pool_info.maxSets = 1000;												// way bigger than needed!
		pool_info.poolSizeCount = static_cast<uint32_t>(std::size(pool_sizes));
		pool_info.pPoolSizes = pool_sizes;
		if (!CheckResult(vkCreateDescriptorPool(m_device->GetVkDevice(), &pool_info, nullptr, &m_vk->m_imgui.m_descriptorPool)))
		{
			LogError("Failed to create descriptor pool for imgui");
			return false;
		}

		// initialise imgui for SDL
		if (!ImGui_ImplSDL2_InitForVulkan(m_mainWindow->GetHandle()))
		{
			LogError("Failed to init imgui for SDL/Vulkan");
			return false;
		}

		// Load custom fonts now
		if (GetSystem<ImGuiSystem>())
		{
			GetSystem<ImGuiSystem>()->LoadFonts();
		}

		// initialise for Vulkan
		// this initializes imgui for Vulkan
		ImGui_ImplVulkan_InitInfo init_info = {0};
		init_info.Instance = m_device->GetVkInstance();
		init_info.PhysicalDevice = m_device->GetPhysicalDevice().m_device;
		init_info.Device = m_device->GetVkDevice();
		init_info.Queue = m_device->GetGraphicsQueue();
		init_info.DescriptorPool = m_vk->m_imgui.m_descriptorPool;
		init_info.MinImageCount = static_cast<uint32_t>(m_swapChain->GetImages().size());	// ??
		init_info.ImageCount = static_cast<uint32_t>(m_swapChain->GetImages().size());		// ????!
		init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
		init_info.UseDynamicRendering = true;
		init_info.ColorAttachmentFormat = m_swapChain->GetFormat().format;
		if (!ImGui_ImplVulkan_Init(&init_info, nullptr))	
		{
			LogError("Failed to init imgui for Vulkan");
			return false;
		}

		// upload the imgui font textures via immediate graphics cmd
		VulkanHelpers::RunCommandsImmediate(m_device->GetVkDevice(), m_device->GetGraphicsQueue(), 
			m_vk->ThisFrameData().m_graphicsCommandPool, m_vk->m_immediateSubmitFence, [&](VkCommandBuffer cmd) {
			ImGui_ImplVulkan_CreateFontsTexture(cmd);
		});
		ImGui_ImplVulkan_DestroyFontUploadObjects();	// destroy the font data once it is uploaded

		return true;
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

		AllocatedBuffer stagingBuffer = VulkanHelpers::CreateBuffer(m_device->GetVMA(),
			sizeof(PosColourVertex) * 3,
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VMA_MEMORY_USAGE_AUTO,
			VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
		);
		m_vk->m_posColourVertexBuffer = VulkanHelpers::CreateBuffer(m_device->GetVMA(),
			sizeof(PosColourVertex) * 3,
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT
		);
		if (m_vk->m_posColourVertexBuffer.m_buffer == VK_NULL_HANDLE || stagingBuffer.m_buffer == VK_NULL_HANDLE)
		{
			LogError("Failed to create buffers");
			return false;
		}

		// upload the data to the staging buffer
		void* mappedData = nullptr;
		if (CheckResult(vmaMapMemory(m_device->GetVMA(), stagingBuffer.m_allocation, &mappedData)))
		{
			memcpy(mappedData, verts, 3 * sizeof(PosColourVertex));
			vmaUnmapMemory(m_device->GetVMA(), stagingBuffer.m_allocation);
		}

		// copy from staging to vertex buffer using immediate cmd submit
		VulkanHelpers::RunCommandsImmediate(m_device->GetVkDevice(), m_device->GetGraphicsQueue(), m_vk->ThisFrameData().m_graphicsCommandPool, m_vk->m_immediateSubmitFence,
			[&](VkCommandBuffer& buf) {
				VkBufferCopy copyRegion{};
				copyRegion.srcOffset = 0;
				copyRegion.dstOffset = 0;
				copyRegion.size = sizeof(PosColourVertex) * 3;
				vkCmdCopyBuffer(buf, stagingBuffer.m_buffer, m_vk->m_posColourVertexBuffer.m_buffer, 1, &copyRegion);
		});

		// done with the staging buffer
		vmaDestroyBuffer(m_device->GetVMA(), stagingBuffer.m_buffer, stagingBuffer.m_allocation);

		return m_vk->m_posColourVertexBuffer.m_buffer != VK_NULL_HANDLE;
	}

	bool RenderSystem::RecreateSwapchain()
	{
		R3_PROF_EVENT();
		CheckResult(vkDeviceWaitIdle(m_device->GetVkDevice()));

		vkDestroyImageView(m_device->GetVkDevice(), m_vk->m_depthBufferView, nullptr);
		vmaDestroyImage(m_device->GetVMA(), m_vk->m_depthBufferImage.m_image, m_vk->m_depthBufferImage.m_allocation);
		m_swapChain->Destroy(*m_device);

		if (!m_swapChain->Initialise(*m_device, *m_mainWindow))
		{
			LogError("Failed to create swapchain");
			return false;
		}

		return CreateDepthBuffer();
	}

	bool RenderSystem::CreateSyncObjects()
	{
		R3_PROF_EVENT();
		// semaphores + fences dont have many params
		VkSemaphoreCreateInfo semaphoreInfo = { 0 };
		semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
		VkFenceCreateInfo fenceInfo = { 0 };
		fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;	// create the fence signalled since we wait on it immediately
		for (int f = 0; f < c_maxFramesInFlight; ++f)
		{
			FrameData& fd = m_vk->m_perFrameData[f];
			if (!CheckResult(vkCreateSemaphore(m_device->GetVkDevice(), &semaphoreInfo, nullptr, &fd.m_imageAvailableSemaphore)))
			{
				LogError("Failed to create semaphore");
				return false;
			}
			if (!CheckResult(vkCreateSemaphore(m_device->GetVkDevice(), &semaphoreInfo, nullptr, &fd.m_renderFinishedSemaphore)))
			{
				LogError("Failed to create semaphore");
				return false;
			}
			if (!CheckResult(vkCreateFence(m_device->GetVkDevice(), &fenceInfo, nullptr, &fd.m_inFlightFence)))
			{
				LogError("Failed to create fence");
				return false;
			}
		}

		VkFenceCreateInfo fenceInfoNotSignalled = fenceInfo;
		fenceInfoNotSignalled.flags = 0;
		if (!CheckResult(vkCreateFence(m_device->GetVkDevice(), &fenceInfoNotSignalled, nullptr, &m_vk->m_immediateSubmitFence)))
		{
			LogError("Failed to create immediate submit fence");
			return false;
		}

		return true;
	}

	void RenderSystem::DrawImgui(int swapImageIndex)
	{
		R3_PROF_EVENT();

		auto& cmdBuffer = m_vk->ThisFrameData().m_graphicsCmdBuffer;

		// Dynamic rendering to colour attachment only
		VkRenderingAttachmentInfo colourAttachment = { 0 };
		colourAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
		colourAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
		colourAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		colourAttachment.imageView = m_swapChain->GetViews()[swapImageIndex];
		colourAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkRenderingInfo ri = { 0 };
		ri.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
		ri.colorAttachmentCount = 1;
		ri.pColorAttachments = &colourAttachment;
		ri.layerCount = 1;
		ri.renderArea.offset = { 0,0 };
		ri.renderArea.extent = m_swapChain->GetExtents();
		vkCmdBeginRendering(cmdBuffer, &ri);

		ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmdBuffer);

		vkCmdEndRendering(cmdBuffer);
	}

	void RenderSystem::RecordMainPass(int swapImageIndex)
	{
		R3_PROF_EVENT();
		auto& cmdBuffer = m_vk->ThisFrameData().m_graphicsCmdBuffer;

		// pipeline dynamic state
		VkViewport viewport = { 0 };
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = (float)m_swapChain->GetExtents().width;
		viewport.height = (float)m_swapChain->GetExtents().height;
		viewport.minDepth = 0.0f;	// normalised! must be between 0 and 1
		viewport.maxDepth = 1.0f;	// ^^
		VkRect2D scissor = { 0 };
		scissor.offset = { 0, 0 };
		scissor.extent = m_swapChain->GetExtents();	// draw the full image

		// Dynamic rendering to colour + depth
		// also clears these buffers!
		VkRenderingAttachmentInfo colourAttachment = { 0 };
		colourAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
		colourAttachment.clearValue = { {{m_mainPassClearColour.x, m_mainPassClearColour.y, m_mainPassClearColour.z, m_mainPassClearColour.w }} };
		colourAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		colourAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		colourAttachment.imageView = m_swapChain->GetViews()[swapImageIndex];
		colourAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;	// this is the layout used during rendering, we need to ensure the transition happens first

		VkRenderingAttachmentInfo depthAttachment = { 0 };
		depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
		depthAttachment.clearValue = { { 1.0f, 0} };
		depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		depthAttachment.imageView = m_vk->m_depthBufferView;
		depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		VkRenderingInfo ri = { 0 };
		ri.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
		ri.colorAttachmentCount = 1;
		ri.pColorAttachments = &colourAttachment;
		ri.pDepthAttachment = &depthAttachment;
		ri.layerCount = 1;
		ri.renderArea.offset = { 0,0 };
		ri.renderArea.extent = m_swapChain->GetExtents();
		vkCmdBeginRendering(cmdBuffer, &ri);

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
		constexpr VkDeviceSize offset = 0;
		vkCmdBindVertexBuffers(cmdBuffer, 0, 1, &m_vk->m_posColourVertexBuffer.m_buffer, &offset);

		// draw one triangle made of 3 verts
		vkCmdDraw(cmdBuffer, 3, 1, 0, 0);

		// bind the pipeline with push constants
		vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_vk->m_simpleTriFromBuffersPushConstantPipeline);

		// Set pipeline dynamic state
		vkCmdSetViewport(cmdBuffer, 0, 1, &viewport);
		vkCmdSetScissor(cmdBuffer, 0, 1, &scissor);

		// bind the vertex buffer with offset 0
		vkCmdBindVertexBuffers(cmdBuffer, 0, 1, &m_vk->m_posColourVertexBuffer.m_buffer, &offset);

		// push constants
		PushConstant constants;
		static double angle = 0.0;
		angle += GetSystem<TimeSystem>()->GetVariableDeltaTime();
		constants.m_colour = { sin(angle), cos(angle), 1.0f, 1.0f };
		constants.m_transform = glm::translate(glm::vec3(sin(angle), sin(angle), sin(angle)));
		vkCmdPushConstants(cmdBuffer, m_vk->m_simpleLayoutWithPushConstant, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(constants), &constants);

		// draw one triangle made of 3 verts
		vkCmdDraw(cmdBuffer, 3, 1, 0, 0);

		vkCmdEndRendering(cmdBuffer);
	}

	bool RenderSystem::RecordCommandBuffer(int swapImageIndex)
	{
		R3_PROF_EVENT();

		auto& cmdBuffer = m_vk->ThisFrameData().m_graphicsCmdBuffer;

		// start writing
		VkCommandBufferBeginInfo beginInfo = { 0 };
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		if (!CheckResult(vkBeginCommandBuffer(cmdBuffer, &beginInfo)))	// resets the cmd buffer
		{
			LogError("failed to begin recording command buffer!");
			return false;
		}

		// Transition swap chain image and depth image to format optimal for drawing 
		{
			auto colourBarrier = VulkanHelpers::MakeImageBarrier(m_swapChain->GetImages()[swapImageIndex],
				VK_IMAGE_ASPECT_COLOR_BIT,
				VK_ACCESS_NONE,	
				VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,		// dst mask = colour write
				VK_IMAGE_LAYOUT_UNDEFINED,					// we dont care what format it starts in
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);	// to format optimal for drawing
			auto depthBarrier = VulkanHelpers::MakeImageBarrier(m_vk->m_depthBufferImage.m_image,
				VK_IMAGE_ASPECT_DEPTH_BIT,
				VK_ACCESS_NONE,
				VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,		// dst mask = depth write
				VK_IMAGE_LAYOUT_UNDEFINED,							// we dont care what format it starts in
				VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);	// to format optimal for drawing
			VkImageMemoryBarrier barriers[] = { colourBarrier, depthBarrier };

			// dst stages = before colour write or depth read
			auto dstStages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
			vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, dstStages, 0, 0, nullptr, 0, nullptr, 2, barriers);
		}

		RecordMainPass(swapImageIndex);
		DrawImgui(swapImageIndex);

		// Transition swap chain image to format optimal for present
		auto colourBarrier = VulkanHelpers::MakeImageBarrier(m_swapChain->GetImages()[swapImageIndex],
			VK_IMAGE_ASPECT_COLOR_BIT, 
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,		// src mask = wait before writing attachment
			VK_ACCESS_NONE,								// dst mask = we dont care?
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,	// from format optimal for drawing
			VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);			// to format optimal for present
		vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &colourBarrier);

		if (!CheckResult(vkEndCommandBuffer(cmdBuffer)))
		{
			LogError("failed to end recording command buffer!");
			return false;
		}

		return true;
	}

	bool RenderSystem::CreateCommandBuffers()
	{
		R3_PROF_EVENT();

		// per-frame cmd buffers
		for (int f = 0; f < c_maxFramesInFlight; ++f)
		{
			VkCommandBufferAllocateInfo allocInfo = {};
			allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
			allocInfo.commandPool = m_vk->m_perFrameData[f].m_graphicsCommandPool;	// allocate from this frame's pool
			allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;		// primary = can be submitted to queue, can't be called from other buffers
																	// secondary = can't be directly submitted, can be called from other primary buffers
			allocInfo.commandBufferCount = 1;
			if (!CheckResult(vkAllocateCommandBuffers(m_device->GetVkDevice(), &allocInfo, &m_vk->m_perFrameData[f].m_graphicsCmdBuffer)))
			{
				LogError("failed to allocate command buffer!");
				return false;
			}
		}

		return true;
	}

	bool RenderSystem::CreateCommandPools()
	{
		R3_PROF_EVENT();
		// find a graphics queue
		auto queueFamilyIndices = FindQueueFamilyIndices(m_device->GetPhysicalDevice(), m_device->GetMainSurface());
		VkCommandPoolCreateInfo poolInfo = {};
		poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;	// - allow individual buffers to be re-recorded
																			// // VK_COMMAND_POOL_CREATE_TRANSIENT_BIT - hint that buffers are re-recorded often
		poolInfo.queueFamilyIndex = queueFamilyIndices.m_graphicsIndex;		// graphics queue pls

		for (int frame = 0; frame < c_maxFramesInFlight; ++frame)
		{
			if (!CheckResult(vkCreateCommandPool(m_device->GetVkDevice(), &poolInfo, nullptr, &m_vk->m_perFrameData[frame].m_graphicsCommandPool)))
			{
				LogError("failed to create command pool!");
				return false;
			}
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
		auto singleTriVertexShader = VulkanHelpers::LoadShaderModule(m_device->GetVkDevice(), basePath + "fixed_triangle.vert.spv");
		auto singleTriFragmentShader = VulkanHelpers::LoadShaderModule(m_device->GetVkDevice(), basePath + "fixed_triangle.frag.spv");
		auto triBufferVertShader = VulkanHelpers::LoadShaderModule(m_device->GetVkDevice(), basePath + "triangle_from_buffers.vert.spv");
		auto triBufferFragShader = VulkanHelpers::LoadShaderModule(m_device->GetVkDevice(), basePath + "triangle_from_buffers.frag.spv");
		auto triBufferPushConstantVertShader = VulkanHelpers::LoadShaderModule(m_device->GetVkDevice(), basePath + "triangle_from_buffers_with_push_constants.vert.spv");
		if (singleTriVertexShader == VK_NULL_HANDLE || singleTriFragmentShader == VK_NULL_HANDLE
			|| triBufferVertShader == VK_NULL_HANDLE || triBufferFragShader == VK_NULL_HANDLE
			|| triBufferPushConstantVertShader == VK_NULL_HANDLE)
		{
			LogError("Failed to create shader modules");
			return false;
		}

		// Create pipeline layouts
		VkPipelineLayoutCreateInfo pipelineLayoutInfo = { 0 };
		pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutInfo.setLayoutCount = 0;
		pipelineLayoutInfo.pushConstantRangeCount = 0;
		if (!CheckResult(vkCreatePipelineLayout(m_device->GetVkDevice(), &pipelineLayoutInfo, nullptr, &m_vk->m_simplePipelineLayout)))
		{
			LogError("Failed to create pipeline layout!");
			return false;
		}

		// push constants specified as byte ranges, bound to specific shader stages
		VkPushConstantRange constantRange;
		constantRange.offset = 0;	// needs to match in the shader if >0!
		constantRange.size = sizeof(PushConstant);
		constantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
		pipelineLayoutInfo.pushConstantRangeCount = 1;
		pipelineLayoutInfo.pPushConstantRanges = &constantRange;
		if (!CheckResult(vkCreatePipelineLayout(m_device->GetVkDevice(), &pipelineLayoutInfo, nullptr, &m_vk->m_simpleLayoutWithPushConstant)))
		{
			LogError("Failed to create pipeline layout!");
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

		// build the pipeline
		m_vk->m_simpleTriPipeline = pb.Build(m_device->GetVkDevice(), m_vk->m_simplePipelineLayout, 1, &m_swapChain->GetFormat().format, m_vk->m_depthBufferFormat);
		if (m_vk->m_simpleTriPipeline == VK_NULL_HANDLE)
		{
			LogError("Failed to create pipeline!");
			return false;
		}

		// dont need the shader modules any more!
		vkDestroyShaderModule(m_device->GetVkDevice(), singleTriFragmentShader, nullptr);
		vkDestroyShaderModule(m_device->GetVkDevice(), singleTriVertexShader, nullptr);

		// build a similar pipeline but with shaders that take vertex data from buffers
		// also passes buffer descriptors!
		pb.m_shaderStages.clear();
		pb.m_shaderStages.push_back(VulkanHelpers::CreatePipelineShaderState(VK_SHADER_STAGE_VERTEX_BIT, triBufferVertShader));
		pb.m_shaderStages.push_back(VulkanHelpers::CreatePipelineShaderState(VK_SHADER_STAGE_FRAGMENT_BIT, triBufferFragShader));

		// bind the buffers + input attrib descriptors
		auto bufferBindingDescriptions = PosColourVertex::GetInputBindingDescription();
		auto attributeDescriptions = PosColourVertex::GetAttributeDescriptions();
		pb.m_vertexInputState.vertexBindingDescriptionCount = 1;
		pb.m_vertexInputState.pVertexBindingDescriptions = &bufferBindingDescriptions;
		pb.m_vertexInputState.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
		pb.m_vertexInputState.pVertexAttributeDescriptions = attributeDescriptions.data();

		m_vk->m_simpleTriFromBuffersPipeline = pb.Build(m_device->GetVkDevice(), m_vk->m_simplePipelineLayout, 1, &m_swapChain->GetFormat().format, m_vk->m_depthBufferFormat);
		if (m_vk->m_simpleTriFromBuffersPipeline == VK_NULL_HANDLE)
		{
			LogError("Failed to create fancier pipeline!");
			return false;
		}
		vkDestroyShaderModule(m_device->GetVkDevice(), triBufferVertShader, nullptr);

		// build pipeline for simple mesh with transform push constant (and push constant layout!!!)
		pb.m_shaderStages.clear();
		pb.m_shaderStages.push_back(VulkanHelpers::CreatePipelineShaderState(VK_SHADER_STAGE_VERTEX_BIT, triBufferPushConstantVertShader));
		pb.m_shaderStages.push_back(VulkanHelpers::CreatePipelineShaderState(VK_SHADER_STAGE_FRAGMENT_BIT, triBufferFragShader));
		m_vk->m_simpleTriFromBuffersPushConstantPipeline = pb.Build(m_device->GetVkDevice(), m_vk->m_simpleLayoutWithPushConstant, 1, &m_swapChain->GetFormat().format, m_vk->m_depthBufferFormat);
		if (m_vk->m_simpleTriFromBuffersPushConstantPipeline == VK_NULL_HANDLE)
		{
			LogError("Failed to create fancier pipeline!");
			return false;
		}
		vkDestroyShaderModule(m_device->GetVkDevice(), triBufferPushConstantVertShader, nullptr);
		vkDestroyShaderModule(m_device->GetVkDevice(), triBufferFragShader, nullptr);

		return true;
	}

	bool RenderSystem::CreateDepthBuffer()
	{
		// Create the image first
		VkImageCreateInfo info = { };
		info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		info.pNext = nullptr;
		info.imageType = VK_IMAGE_TYPE_2D;
		info.format = VK_FORMAT_D32_SFLOAT;
		info.extent = {
			m_swapChain->GetExtents().width, m_swapChain->GetExtents().height, 1
		};	// same size as swap chain images
		info.mipLevels = 1;
		info.arrayLayers = 1;
		info.samples = VK_SAMPLE_COUNT_1_BIT;
		info.tiling = VK_IMAGE_TILING_OPTIMAL;
		info.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;	// depth-stencil attachment

		// We want the allocation to be in fast gpu memory!
		VmaAllocationCreateInfo allocInfo = { };
		allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
		allocInfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		auto r = vmaCreateImage(m_device->GetVMA(), &info, &allocInfo, &m_vk->m_depthBufferImage.m_image, &m_vk->m_depthBufferImage.m_allocation, nullptr);
		if (!CheckResult(r))
		{
			return false;
		}
		m_vk->m_depthBufferFormat = info.format;	// is this actually correct? is the requested format what we actually get?

		// Create an ImageView for the depth buffer
		VkImageViewCreateInfo vci = {};
		vci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		vci.viewType = VK_IMAGE_VIEW_TYPE_2D;
		vci.image = m_vk->m_depthBufferImage.m_image;
		vci.format = m_vk->m_depthBufferFormat;
		vci.subresourceRange.baseMipLevel = 0;
		vci.subresourceRange.levelCount = 1;
		vci.subresourceRange.baseArrayLayer = 0;
		vci.subresourceRange.layerCount = 1;
		vci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;	// we want to access the depth data
		r = vkCreateImageView(m_device->GetVkDevice(), &vci, nullptr, &m_vk->m_depthBufferView);
		if (!CheckResult(r))
		{
			return false;
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
}