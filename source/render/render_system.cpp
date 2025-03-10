#include "render_system.h"
#include "window.h"
#include "device.h"
#include "swap_chain.h"
#include "render_target_cache.h"
#include "render_graph.h"
#include "core/profiler.h"
#include "core/log.h"
#include "core/platform.h"
#include "engine/systems/event_system.h"
#include "engine/systems/time_system.h"
#include "engine/ui/imgui_menubar_helper.h"
#include "vulkan_helpers.h"
#include "buffer_pool.h"
#include "pipeline_builder.h"
#include "command_buffer_allocator.h"
#include "timestamp_queries_handler.h"
#include <vulkan/vk_enum_string_helper.h>
#include <SDL.h>
#include <SDL_events.h>
#include <imgui.h>
#include <vma/vk_mem_alloc.h>

namespace R3
{
	static constexpr int c_maxFramesInFlight = 2;	// let the cpu get ahead of the gpu by this many frames. should always be > num swap images
	using VulkanHelpers::CheckResult;				// laziness
	using VulkanHelpers::FindQueueFamilyIndices;

	struct FrameData
	{
		VkSemaphore m_imageAvailableSemaphore = VK_NULL_HANDLE;	// signalled when new swap chain image available
		VkSemaphore m_renderFinishedSemaphore = VK_NULL_HANDLE;	// signalled when m_graphicsCmdBuffer has been fully submitted to a queue
		VkFence m_inFlightFence = VK_NULL_HANDLE;				// signalled when previous cmd buffer finished executing (initialised as signalled)
		DeletionQueue m_deleters;								// queue of objects to be deleted this frame
		TimestampQueriesHandler m_timestamps;					// per-frame timestamp handler for profiling
	};

	struct AllocatedImage
	{
		VkImage m_image;
		VmaAllocation m_allocation;
	};

	struct RenderSystem::VkStuff
	{
		FrameData m_perFrameData[c_maxFramesInFlight];	// contains per frame cmd buffers, sync objects
		int m_currentFrame = 0;
		VkFence m_immediateSubmitFence = VK_NULL_HANDLE;
		FrameData& ThisFrameData()
		{
			assert(m_currentFrame < c_maxFramesInFlight);
			return m_perFrameData[m_currentFrame];
		}
	};

	RenderSystem::RenderSystem()
	{
		m_vk = std::make_unique<VkStuff>();
		m_bufferPool = std::make_unique<BufferPool>("Global buffer pool", 16 * 1024 * 1024);
		m_mainDeleters.PushDeleter([this]() {
			m_bufferPool = nullptr;
		});
		m_cmdBufferAllocator = std::make_unique<CommandBufferAllocator>();
		m_mainDeleters.PushDeleter([this]() {
			m_cmdBufferAllocator = nullptr;
		});
		m_renderGraph = std::make_unique<RenderGraph>();
	}

	RenderSystem::~RenderSystem()
	{
		m_vk = nullptr;
	}

	RenderTargetInfo RenderSystem::GetSwapchainTargetInfo()
	{
		RenderTargetInfo swapInfo;
		swapInfo.m_format = m_swapChain->GetFormat().format;
		swapInfo.m_name = "Swapchain";
		swapInfo.m_size = { m_swapChain->GetExtents().width, m_swapChain->GetExtents().height };
		swapInfo.m_sizeType = RenderTargetInfo::SizeType::Fixed;
		return swapInfo;
	}

	RenderTargetCache* RenderSystem::GetRenderTargetCache()
	{
		return m_renderTargets.get();
	}

	void RenderSystem::RunGraph(RenderGraph& r, VkCommandBuffer_T* cmdBuffer, VkImage_T* swapImage, VkImageView_T* swapImageView)
	{
		R3_PROF_EVENT();

		VkCommandBufferBeginInfo beginInfo = { 0 };
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		if (!CheckResult(vkBeginCommandBuffer(cmdBuffer, &beginInfo)))	// resets the cmd buffer
		{
			LogError("failed to begin recording command buffer!");
			return;
		}
		{
			auto& timestamps = m_vk->ThisFrameData().m_timestamps;
			timestamps.Reset(cmdBuffer);	// reset timestamp query pool for this frame
			R3_PROF_GPU_COMMANDS(cmdBuffer);
			R3_PROF_GPU_EVENT("RunGraph");

			auto thisFrameTime = timestamps.MakeScopedQuery("Total Render Time");

			// Pass the current swap chain info to the target cache (so it can be accessed from the graph)
			RenderTargetInfo swapInfo = GetSwapchainTargetInfo();
			m_renderTargets->AddTarget(swapInfo, swapImage, swapImageView);

			RenderGraph::GraphContext ctx;
			ctx.m_device = m_device.get();
			ctx.m_graphicsCmds = cmdBuffer;
			ctx.m_targets = m_renderTargets.get();
			ctx.m_timestampHandler = &timestamps;
			ctx.m_targets->ResetForNewFrame();
			m_renderGraph->Run(ctx);

			// Transition swap chain image to format optimal for present
			auto colourBarrier = VulkanHelpers::MakeImageBarrier(swapImage,
				VK_IMAGE_ASPECT_COLOR_BIT,
				VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,				// src mask = wait before writing attachment
				VK_ACCESS_NONE,										// dst mask = we dont care?
				VK_IMAGE_LAYOUT_UNDEFINED,							// we don't really care what the previous layout is
				VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);					// to format optimal for present
			vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &colourBarrier);
		}
		if (!CheckResult(vkEndCommandBuffer(cmdBuffer)))
		{
			LogError("failed to end recording command buffer!");
		}
	}

	glm::vec2 RenderSystem::GetWindowExtents()
	{
		auto e = m_swapChain->GetExtents();
		return { e.width, e.height };
	}

	Device* RenderSystem::GetDevice()
	{
		return m_device.get();
	}

	Window* RenderSystem::GetMainWindow()
	{
		return m_mainWindow.get();
	}

	Swapchain* RenderSystem::GetSwapchain()
	{
		return m_swapChain.get();
	}

	BufferPool* RenderSystem::GetBufferPool()
	{
		return m_bufferPool.get();
	}

	CommandBufferAllocator* RenderSystem::GetCommandBufferAllocator()
	{
		return m_cmdBufferAllocator.get();
	}

	void RenderSystem::RegisterTickFns()
	{
		R3_PROF_EVENT();
		RegisterTick("Render::DrawFrame", [this]() {
			return DrawFrame();
		});
		RegisterTick("Render::AcquireSwapImage", [this]() {
			return AcquireSwapImage();
		});
		RegisterTick("Render::ShowGui", [this]() {
			return ShowGui();
		});
	}

	void RenderSystem::ShowGpuProfilerGui()
	{
		R3_PROF_EVENT();
		auto scopedTimestampValues = m_vk->ThisFrameData().m_timestamps.GetScopedResults();
		for (const auto& result : scopedTimestampValues)
		{
			std::string txt = std::format("{} - {:.3f}ms", result.m_name, result.m_endTime - result.m_startTime);
			ImGui::Text(txt.c_str());
		}
	}

	bool RenderSystem::ShowGui()
	{
		R3_PROF_EVENT();

		auto timeSys = GetSystem<TimeSystem>();
		auto str = std::format("FPS/Frame Time: {:.1f} / {:.4f}ms", 1.0 / timeSys->GetVariableDeltaTime(), timeSys->GetVariableDeltaTime());
		auto strSizePixels = ImGui::CalcTextSize(str.data());
		ImGui::GetForegroundDrawList()->AddText({ GetWindowExtents().x - strSizePixels.x - 2,2}, 0xffffffff, str.c_str());

		return true;
	}

	bool RenderSystem::CreateSwapchain()
	{
		m_swapChain = std::make_unique<Swapchain>();
		return m_swapChain->Initialise(*m_device, *m_mainWindow, ShouldEnableVsync());
	}

	bool RenderSystem::ShouldEnableVsync()
	{
		bool useVsync = Platform::GetCmdLine().find("-vsync") != std::string::npos;
		useVsync |= Platform::GetSystemPowerState().m_isRunningOnBattery;	// enable vsync if running on battery
		return useVsync;
	}

	bool RenderSystem::PrepareSwapchain()
	{
		R3_PROF_EVENT();
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

	bool RenderSystem::AcquireSwapImage()
	{
		R3_PROF_EVENT();

		// Nothing to draw this frame if this returns false
		if (!PrepareSwapchain())
		{
			return true;
		}

		// If running with low battery, wait a bit!
		auto powerState = Platform::GetSystemPowerState();
		if (powerState.m_isRunningOnBattery && powerState.m_batteryPercentageRemaining < 90)
		{
			R3_PROF_EVENT("StopKillingBattery");
			SDL_Delay(10);
		}

		auto& fd = m_vk->ThisFrameData();
		{
			R3_PROF_STALL("Wait for previous frame fence");
			// wait for the previous frame to finish with infinite timeout (blocking call)
			CheckResult(vkWaitForFences(m_device->GetVkDevice(), 1, &fd.m_inFlightFence, VK_TRUE, UINT64_MAX));
		}

		{
			R3_PROF_EVENT("Per Frame Deleters");
			fd.m_deleters.DeleteAll();		// Run when we know for sure the previous frame completed
		}

		m_currentSwapImage = -1;
		{
			R3_PROF_EVENT("Acquire swap image");
			// acquire the next swap chain image (blocks with infinite timeout)
			// m_imageAvailableSemaphore will be signalled when we are able to draw to the image
			// note that fences can also be passed here
			auto r = vkAcquireNextImageKHR(m_device->GetVkDevice(), m_swapChain->GetSwapchain(), UINT64_MAX, fd.m_imageAvailableSemaphore, VK_NULL_HANDLE, &m_currentSwapImage);
			if (r == VK_ERROR_OUT_OF_DATE_KHR)
			{
				m_recreateSwapchain = true;
				m_currentSwapImage = -1;
				return true; // dont continue
			}
			else if (!CheckResult(r))
			{
				LogError("Failed to aqcuire next swap chain image index");
				m_currentSwapImage = -1;
				return false;
			}
		}

		// at this point we are definitely going to draw, so reset the inflight fence
		CheckResult(vkResetFences(m_device->GetVkDevice(), 1, &fd.m_inFlightFence));

		return true;
	}

	bool RenderSystem::DrawFrame()
	{
		R3_PROF_EVENT();

		// Nothing to draw this frame if this returns false
		if (!PrepareSwapchain() || m_currentSwapImage == -1)
		{
			return true;
		}
		
		auto graphicsCmds = m_cmdBufferAllocator->CreateCommandBuffer(*m_device, true, "Main Graphics Commands");
		if (!graphicsCmds)
		{
			LogError("Failed to create cmd buffer!");
			return false;
		}

		auto& fd = m_vk->ThisFrameData();

		fd.m_timestamps.CollectResults(*m_device);	// cache results for the previous frame queries
		RunGraph(*m_renderGraph, graphicsCmds->m_cmdBuffer, m_swapChain->GetImages()[m_currentSwapImage], m_swapChain->GetViews()[m_currentSwapImage]);

		// submit the cmd buffer to the graphics queue
		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.pCommandBuffers = &graphicsCmds->m_cmdBuffer;	// which buffer to submit
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
		VkPresentInfoKHR presentInfo = {};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		presentInfo.pNext = nullptr;
		presentInfo.pSwapchains = &m_swapChain->GetSwapchain();
		presentInfo.swapchainCount = 1;
		presentInfo.pWaitSemaphores = &fd.m_renderFinishedSemaphore;		// wait on this semaphore before presenting
		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pImageIndices = &m_currentSwapImage;
		{
			R3_PROF_EVENT("Present");
			R3_PROF_GPU_FLIP(m_swapChain->GetSwapchain());
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

		// release the command buffer back to the pool
		m_cmdBufferAllocator->Release(*graphicsCmds);

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
		constexpr bool c_enableDynamicRendering = true;		// allows us to bypass renderpasses/subpasses
		bool enableValidationLayers = Platform::GetCmdLine().find("-debugvulkan") != std::string::npos;
		m_device = std::make_unique<Device>(m_mainWindow.get());
		if (!m_device->Initialise(enableValidationLayers, c_enableDynamicRendering))
		{
			LogError("Failed to initialise device");
			return false;
		}
		return VulkanHelpers::Extensions::Initialise(m_device->GetVkDevice());
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
		if (!CreateSyncObjects())
		{
			LogError("Failed to create sync objects");
			return false;
		}
		if (!CreateTimestampHandlers())
		{
			LogError("Failed to create timestamp handlers");
			return false;
		}

		m_renderTargets = std::make_unique<RenderTargetCache>();
		m_mainDeleters.PushDeleter([&]() {
			m_renderTargets = {};
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

		// Run all registered shutdown callbacks
		m_onShutdownCbs.Run(*m_device);

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

	void RenderSystem::RunGraphicsCommandsImmediate(std::function<void(VkCommandBuffer)> fn)
	{
		VulkanHelpers::RunCommandsImmediate(m_device->GetVkDevice(), 
			m_device->GetGraphicsQueue(),
			m_cmdBufferAllocator->GetPool(*m_device), 
			m_vk->m_immediateSubmitFence, 
			fn);
	}

	bool RenderSystem::RecreateSwapchain()
	{
		R3_PROF_EVENT();
		CheckResult(vkDeviceWaitIdle(m_device->GetVkDevice()));

		// Destroy old depth/backbuffer/swapchain images
		m_renderTargets->Clear();
		m_swapChain->Destroy(*m_device);

		if (!m_swapChain->Initialise(*m_device, *m_mainWindow, ShouldEnableVsync()))
		{
			LogError("Failed to create swapchain");
			return false;
		}
		return true;
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

	bool RenderSystem::CreateTimestampHandlers()
	{
		R3_PROF_EVENT();

		for (int f = 0; f < c_maxFramesInFlight; ++f)
		{
			FrameData& fd = m_vk->m_perFrameData[f];
			fd.m_timestamps.Initialise(*m_device, 128);

			m_mainDeleters.PushDeleter([&, f]() 
			{
				m_vk->m_perFrameData[f].m_timestamps.Cleanup(*m_device);
			});
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