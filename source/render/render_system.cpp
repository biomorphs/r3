#include "render_system.h"
#include "window.h"
#include "device.h"
#include "swap_chain.h"
#include "immediate_renderer.h"
#include "camera.h"
#include "core/file_io.h"
#include "core/profiler.h"
#include "core/log.h"
#include "engine/systems/event_system.h"
#include "engine/systems/time_system.h"
#include "engine/systems/imgui_system.h"
#include "engine/systems/camera_system.h"
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
		DeletionQueue m_deleters;								// queue of objects to be deleted this frame
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
		AllocatedImage m_backBufferImage;
		VkImageView m_backBufferView;
		VkFormat m_backBufferFormat;
		AllocatedImage m_depthBufferImage;	// note the tutorial may be wrong here, we might want one per swap chain image!
		VkImageView m_depthBufferView;
		VkFormat m_depthBufferFormat;
		FrameData m_perFrameData[c_maxFramesInFlight];	// contains per frame cmd buffers, sync objects
		int m_currentFrame = 0;
		VkFence m_immediateSubmitFence = VK_NULL_HANDLE;
		ImGuiVkStuff m_imgui;

		// helpers
		FrameData& ThisFrameData()
		{
			assert(m_currentFrame < c_maxFramesInFlight);
			return m_perFrameData[m_currentFrame];
		}
	};

	RenderSystem::RenderSystem()
	{
		m_vk = std::make_unique<VkStuff>();
	}

	RenderSystem::~RenderSystem()
	{
		m_vk = nullptr;
	}

	void RenderSystem::DrawImgui(int swapImageIndex)
	{
		R3_PROF_EVENT();

		auto& cmdBuffer = m_vk->ThisFrameData().m_graphicsCmdBuffer;

		// rendering to colour attachment only
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

		// push immediate renderer vertices to gpu outside of render pass 
		m_imRenderer->WriteVertexData(*m_device, cmdBuffer);

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

		// rendering to colour + depth
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

		// IM renderer draw
		auto cameras = GetSystem<CameraSystem>();
		m_imRenderer->Draw(cameras->GetMainCamera().ProjectionMatrix() * cameras->GetMainCamera().ViewMatrix(), *m_device, *m_swapChain, cmdBuffer);

		vkCmdEndRendering(cmdBuffer);

		m_imRenderer->Flush();
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

		glm::vec4 groundColour(0.7, 0.7, 0.7, 1.0);
		ImmediateRenderer::PerVertexData testScene[] = {
			{{-10,0,-10,1},groundColour},
			{{10,0,-10,1},groundColour},
			{{10,0,10,1},groundColour},
			{{-10,0,-10,1},groundColour},
			{{10,0,10,1},groundColour},
			{{-10,0,10,1},groundColour}
		};
		int triCount = (sizeof(testScene) / sizeof(testScene[0])) / 3;
		for (int t = 0; t < triCount; ++t)
		{
			m_imRenderer->AddTriangle(&testScene[t * 3]);
		}

		glm::vec4 lineColour(1.0, 0.0, 0.0, 1.0);
		ImmediateRenderer::PerVertexData testLines[] = {
			{ {-1,-1,-1,1}, lineColour},
			{ {1,-1,-1,1}, lineColour},
		
			{ {1,-1,-1,1}, lineColour},
			{ {1,1,-1,1}, lineColour},
		
			{ {1,1,-1,1}, lineColour},
			{ {-1,1,-1,1}, lineColour},
		
			{ {-1,1,-1,1}, lineColour},
			{ {-1,-1,-1,1}, lineColour},

			{ {-1,-1,1,1}, lineColour},
			{ {1,-1,1,1}, lineColour},

			{ {1,-1,1,1}, lineColour},
			{ {1,1,1,1}, lineColour},

			{ {1,1,1,1}, lineColour},
			{ {-1,1,1,1}, lineColour},

			{ {-1,1,1,1}, lineColour},
			{ {-1,-1,1,1}, lineColour},

			{ {-1,-1,-1,1}, lineColour},
			{ {-1,-1,1,1}, lineColour},

			{ {1,-1,-1,1}, lineColour},
			{ {1,-1,1,1}, lineColour},

			{ {1,1,-1,1}, lineColour},
			{ {1,1,1,1}, lineColour},

			{ {-1,1,-1,1}, lineColour},
			{ {-1,1,1,1}, lineColour},
		};
		int lineCount = (sizeof(testLines) / sizeof(testLines[0])) / 2;
		glm::vec3 offset(-32.0f, 1.0f, -2.0f);
		for (int y = 0; y < 32; ++y)
		{
			for (int x = 0; x < 16; ++x)
			{
				glm::mat4 cubeTransform = glm::translate(glm::identity<glm::mat4>(), offset + (glm::vec3(4, 0, 0) * (float)x) + (glm::vec3(0, 4, 0) * (float)y));
				glm::vec4 colourOffset = (glm::vec4(0, 0, 1, 0) * (x / 16.0f)) + (glm::vec4(0, 1, 0, 0) * (y / 32.0f)) + (glm::vec4(-1, 0, 0, 0) * (x / 16.0f));
				m_imRenderer->AddCubeWireframe(cubeTransform, lineColour + colourOffset);
			}
		}
		
		float y = (float)sin(GetSystem<TimeSystem>()->GetElapsedTime());
		ImmediateRenderer::PerVertexData vertices[3];
		vertices[0].m_position = { -1, -1 + y, 0, 1 };		vertices[0].m_colour = { 1,0,0,1 };
		vertices[1].m_position = { 1, -1 + y, 0, 1 };		vertices[1].m_colour = { 0,0,1,1 };
		vertices[2].m_position = { 1, 1 + y, 0, 1 };		vertices[2].m_colour = { 0,1,0,1 };
		m_imRenderer->AddTriangle(vertices);

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
			
			// run any deleters that need to happen this frame
			fd.m_deleters.DeleteAll();
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
		if (!CreateBackBuffer())
		{
			LogError("Failed to create back buffer");
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

		m_imRenderer = std::make_unique<ImmediateRenderer>();
		if (!m_imRenderer->Initialise(*m_device, *m_swapChain, m_vk->m_depthBufferFormat))
		{
			LogError("Failed to create immediate renderer");
			return false;
		}
		m_mainDeleters.PushDeleter([&]() {
			m_imRenderer->Destroy(*m_device);
			m_imRenderer = nullptr;
		});

		m_mainWindow->Show();

		return true;
	}

	void RenderSystem::Shutdown()
	{
		R3_PROF_EVENT();

		m_mainWindow->Hide();

		// Wait for rendering to finish before shutting down
		CheckResult(vkDeviceWaitIdle(m_device->GetVkDevice()));

		// run all per-frame deleters
		for (int f = c_maxFramesInFlight - 1; f >= 0; --f)	// run in reverse order
		{
			m_vk->m_perFrameData[f].m_deleters.DeleteAll();
		}

		// Run all global deleters
		m_mainDeleters.DeleteAll();

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

		m_mainDeleters.PushDeleter([&]() {
			// Shut downn imgui
			vkDestroyDescriptorPool(m_device->GetVkDevice(), m_vk->m_imgui.m_descriptorPool, nullptr);
			ImGui_ImplVulkan_Shutdown();
		});

		return true;
	}

	bool RenderSystem::RecreateSwapchain()
	{
		R3_PROF_EVENT();
		CheckResult(vkDeviceWaitIdle(m_device->GetVkDevice()));

		// Destroy old depth/backbuffer/swapchain images
		vkDestroyImageView(m_device->GetVkDevice(), m_vk->m_depthBufferView, nullptr);
		vmaDestroyImage(m_device->GetVMA(), m_vk->m_depthBufferImage.m_image, m_vk->m_depthBufferImage.m_allocation);
		vkDestroyImageView(m_device->GetVkDevice(), m_vk->m_backBufferView, nullptr);
		vmaDestroyImage(m_device->GetVMA(), m_vk->m_backBufferImage.m_image, m_vk->m_backBufferImage.m_allocation);
		m_swapChain->Destroy(*m_device);

		if (!m_swapChain->Initialise(*m_device, *m_mainWindow))
		{
			LogError("Failed to create swapchain");
			return false;
		}

		bool created = CreateDepthBuffer();
		created &= CreateBackBuffer();
		return created;
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
			m_mainDeleters.PushDeleter([&, f]() {
				vkDestroySemaphore(m_device->GetVkDevice(), m_vk->m_perFrameData[f].m_imageAvailableSemaphore, nullptr);
			});
			if (!CheckResult(vkCreateSemaphore(m_device->GetVkDevice(), &semaphoreInfo, nullptr, &fd.m_renderFinishedSemaphore)))
			{
				LogError("Failed to create semaphore");
				return false;
			}
			m_mainDeleters.PushDeleter([&, f]() {
				vkDestroySemaphore(m_device->GetVkDevice(), m_vk->m_perFrameData[f].m_renderFinishedSemaphore, nullptr);
			});
			if (!CheckResult(vkCreateFence(m_device->GetVkDevice(), &fenceInfo, nullptr, &fd.m_inFlightFence)))
			{
				LogError("Failed to create fence");
				return false;
			}
			m_mainDeleters.PushDeleter([&, f]() {
				vkDestroyFence(m_device->GetVkDevice(), m_vk->m_perFrameData[f].m_inFlightFence, nullptr);
			});
		}

		VkFenceCreateInfo fenceInfoNotSignalled = fenceInfo;
		fenceInfoNotSignalled.flags = 0;
		if (!CheckResult(vkCreateFence(m_device->GetVkDevice(), &fenceInfoNotSignalled, nullptr, &m_vk->m_immediateSubmitFence)))
		{
			LogError("Failed to create immediate submit fence");
			return false;
		}
		m_mainDeleters.PushDeleter([&]() {
			vkDestroyFence(m_device->GetVkDevice(), m_vk->m_immediateSubmitFence, nullptr);
		});
		
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
			m_mainDeleters.PushDeleter([&, frame]() {
				// cmd buffers do not need to be destroyed, removing the pool is enough
				vkDestroyCommandPool(m_device->GetVkDevice(), m_vk->m_perFrameData[frame].m_graphicsCommandPool, nullptr);
			});
		}

		return true;
	}

	bool RenderSystem::CreateDepthBuffer()
	{
		VkExtent3D extents = {
			m_swapChain->GetExtents().width, m_swapChain->GetExtents().height, 1
		};
		VkImageCreateInfo info = VulkanHelpers::CreateImage2DNoMSAANoMips(VK_FORMAT_D32_SFLOAT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, extents);

		// We want the allocation to be in fast gpu memory!
		VmaAllocationCreateInfo allocInfo = { };
		allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
		allocInfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		auto r = vmaCreateImage(m_device->GetVMA(), &info, &allocInfo, &m_vk->m_depthBufferImage.m_image, &m_vk->m_depthBufferImage.m_allocation, nullptr);
		if (!CheckResult(r))
		{
			return false;
		}
		m_mainDeleters.PushDeleter([&]() {
			if (m_vk->m_depthBufferImage.m_image && m_vk->m_depthBufferImage.m_allocation)	// gets queued multiple times due to resize logic
			{
				vmaDestroyImage(m_device->GetVMA(), m_vk->m_depthBufferImage.m_image, m_vk->m_depthBufferImage.m_allocation);
				m_vk->m_depthBufferImage.m_image = nullptr;
				m_vk->m_depthBufferImage.m_allocation = nullptr;
			}
		});
		m_vk->m_depthBufferFormat = info.format;

		// Create an ImageView for the depth buffer
		VkImageViewCreateInfo vci = VulkanHelpers::CreateImageView2DNoMSAANoMips(m_vk->m_depthBufferFormat, m_vk->m_depthBufferImage.m_image, VK_IMAGE_ASPECT_DEPTH_BIT);
		r = vkCreateImageView(m_device->GetVkDevice(), &vci, nullptr, &m_vk->m_depthBufferView);
		if (!CheckResult(r))
		{
			return false;
		}
		m_mainDeleters.PushDeleter([&]() {
			if (m_vk->m_depthBufferView)
			{
				vkDestroyImageView(m_device->GetVkDevice(), m_vk->m_depthBufferView, nullptr);
				m_vk->m_depthBufferView = nullptr;
			}
		});

		return true;
	}

	bool RenderSystem::CreateBackBuffer()
	{
		VkExtent3D extents = {
			m_swapChain->GetExtents().width, m_swapChain->GetExtents().height, 1
		};

		// back buffer is a src + dest for writes/reads, used as colour attachment
		VkImageUsageFlags drawImageUsages{};
		drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
		drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		drawImageUsages |= VK_IMAGE_USAGE_STORAGE_BIT;
		drawImageUsages |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

		// Create the image first
		VkImageCreateInfo info = VulkanHelpers::CreateImage2DNoMSAANoMips(VK_FORMAT_R16G16B16A16_SFLOAT, drawImageUsages, extents);

		// We want the allocation to be in fast gpu memory!
		VmaAllocationCreateInfo allocInfo = { };
		allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
		allocInfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		auto r = vmaCreateImage(m_device->GetVMA(), &info, &allocInfo, &m_vk->m_backBufferImage.m_image, &m_vk->m_backBufferImage.m_allocation, nullptr);
		if (!CheckResult(r))
		{
			return false;
		}
		m_mainDeleters.PushDeleter([&]() {
			if (m_vk->m_backBufferImage.m_image && m_vk->m_backBufferImage.m_allocation)	// gets queued multiple times due to resize logic
			{
				vmaDestroyImage(m_device->GetVMA(), m_vk->m_backBufferImage.m_image, m_vk->m_backBufferImage.m_allocation);
				m_vk->m_backBufferImage.m_image = nullptr;
				m_vk->m_backBufferImage.m_allocation = nullptr;
			}
		});
		m_vk->m_backBufferFormat = info.format;

		// Create an ImageView for the back buffer
		VkImageViewCreateInfo vci = VulkanHelpers::CreateImageView2DNoMSAANoMips(m_vk->m_backBufferFormat, m_vk->m_backBufferImage.m_image, VK_IMAGE_ASPECT_COLOR_BIT);
		r = vkCreateImageView(m_device->GetVkDevice(), &vci, nullptr, &m_vk->m_backBufferView);
		if (!CheckResult(r))
		{
			return false;
		}
		m_mainDeleters.PushDeleter([&]() {
			if (m_vk->m_backBufferView)
			{
				vkDestroyImageView(m_device->GetVkDevice(), m_vk->m_backBufferView, nullptr);
				m_vk->m_backBufferView = nullptr;
			}
			
		});

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