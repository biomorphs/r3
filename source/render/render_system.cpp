#include "render_system.h"
#include "window.h"
#include "device.h"
#include "swap_chain.h"
#include "immediate_renderer.h"
#include "render_target_cache.h"
#include "render_graph.h"
#include "camera.h"
#include "core/file_io.h"
#include "core/profiler.h"
#include "core/log.h"
#include "core/platform.h"
#include "engine/systems/event_system.h"
#include "engine/systems/time_system.h"
#include "engine/systems/imgui_system.h"
#include "engine/systems/camera_system.h"
#include "entities/systems/entity_system.h"
#include "entities/queries.h"
#include "engine/components/environment_settings.h"
#include "engine/model_data.h"
#include "vulkan_helpers.h"
#include "buffer_pool.h"
#include "pipeline_builder.h"
#include "command_buffer_allocator.h"
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
		m_stagingBuffers = std::make_unique<BufferPool>();
		m_mainDeleters.PushDeleter([this]() {
			m_stagingBuffers = nullptr;
		});
		m_cmdBufferAllocator = std::make_unique<CommandBufferAllocator>();
		m_mainDeleters.PushDeleter([this]() {
			m_cmdBufferAllocator = nullptr;
		});
	}

	RenderSystem::~RenderSystem()
	{
		m_vk = nullptr;
	}

	VkFormat RenderSystem::GetMainColourTargetFormat()
	{
		return VK_FORMAT_R16G16B16A16_SFLOAT;
	}

	VkFormat RenderSystem::GetMainDepthStencilFormat()
	{
		return VK_FORMAT_D32_SFLOAT;
	}

	bool RenderSystem::CreateRenderGraph()
	{
		m_renderGraph = std::make_unique<RenderGraph>();
		m_mainDeleters.PushDeleter([this]() {
			m_renderGraph = nullptr;
			});

		// special case, just used as a proxy for lookups later
		RenderTargetInfo swapchainImage("Swapchain");

		// HDR colour buffer is used as src for transfers (blit to swap chain) + used as colour attachment
		RenderTargetInfo mainColour("MainColour");
		mainColour.m_format = GetMainColourTargetFormat();
		mainColour.m_usageFlags = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

		// Main depth buffer
		RenderTargetInfo mainDepth("MainDepth");
		mainDepth.m_format = GetMainDepthStencilFormat();
		mainDepth.m_usageFlags = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
		mainDepth.m_aspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT;

		// 2nd attempt
		auto mainPass = std::make_unique<DrawPass>();			// clear + draw to colour + depth
		mainPass->m_colourAttachments.push_back({ mainColour, DrawPass::AttachmentLoadOp::Clear });
		mainPass->m_depthAttachment = { mainDepth, DrawPass::AttachmentLoadOp::Clear };
		mainPass->m_onBegin.AddCallback([this](RenderPassContext& ctx) {
			m_onMainPassBegin.Run(*m_device, ctx.m_graphicsCmds);
			m_imRenderer->WriteVertexData(*m_device, *m_stagingBuffers.get(), ctx.m_graphicsCmds);
			});
		mainPass->m_onDraw.AddCallback([this](RenderPassContext& ctx) {
			m_onMainPassDraw.Run(*m_device, ctx.m_graphicsCmds, VkExtent2D{ (uint32_t)ctx.m_renderExtents.x, (uint32_t)ctx.m_renderExtents.y });	// refactor?
			auto cameras = GetSystem<CameraSystem>();	// IM renderer draws to colour buffer
			m_imRenderer->Draw(cameras->GetMainCamera().ProjectionMatrix() * cameras->GetMainCamera().ViewMatrix(), *m_device, m_swapChain->GetExtents(), ctx.m_graphicsCmds);
			});
		mainPass->m_onEnd.AddCallback([this](RenderPassContext& ctx) {
			m_onMainPassEnd.Run(*m_device);
			m_imRenderer->Flush();
			});
		mainPass->m_getExtentsFn = [this]() -> glm::vec2 {
			return GetWindowExtents();
			};
		mainPass->m_getClearColourFn = [this]() -> glm::vec4 {
			return m_mainPassClearColour;
			};
		m_renderGraph->m_allPasses.push_back(std::move(mainPass));

		auto blitPass = std::make_unique<TransferPass>();		// blit HDR colour to swap chain
		blitPass->m_inputs.push_back(mainColour);
		blitPass->m_outputs.push_back(swapchainImage);
		blitPass->m_onRun.AddCallback([this, mainColour, swapchainImage](RenderPassContext& ctx) {
			VulkanHelpers::BlitColourImageToImage(ctx.m_graphicsCmds,
			ctx.GetResolvedTarget(mainColour)->m_image, m_swapChain->GetExtents(),
			ctx.GetResolvedTarget(swapchainImage)->m_image, m_swapChain->GetExtents());
			});
		m_renderGraph->m_allPasses.push_back(std::move(blitPass));

		auto imguiPass = std::make_unique<DrawPass>();			// imgui draw direct to swap image
		imguiPass->m_colourAttachments.push_back({ swapchainImage, DrawPass::AttachmentLoadOp::Load });
		imguiPass->m_getExtentsFn = [this]() -> glm::vec2 {
			return GetWindowExtents();
			};
		imguiPass->m_onDraw.AddCallback([this](RenderPassContext& ctx) {
			ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), ctx.m_graphicsCmds);
			});
		m_renderGraph->m_allPasses.push_back(std::move(imguiPass));

		return true;
	}

	void RenderSystem::RunGraph(RenderGraph& r, VkCommandBuffer_T* cmdBuffer, VkImage_T* swapImage, VkImageView_T* swapImageView)
	{
		R3_PROF_EVENT();

		// start writing
		VkCommandBufferBeginInfo beginInfo = { 0 };
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		if (!CheckResult(vkBeginCommandBuffer(cmdBuffer, &beginInfo)))	// resets the cmd buffer
		{
			LogError("failed to begin recording command buffer!");
			return;
		}

		// Pass the current swap chain info to the target cache (so it can be accessed from the graph)
		RenderTargetInfo swapInfo("Swapchain");
		swapInfo.m_format = m_swapChain->GetFormat().format;
		swapInfo.m_name = "Swapchain";
		swapInfo.m_size = { m_swapChain->GetExtents().width, m_swapChain->GetExtents().height };
		swapInfo.m_sizeType = RenderTargetInfo::SizeType::Fixed;
		m_renderTargets->AddTarget(swapInfo, swapImage, swapImageView);

		RenderGraph::GraphContext ctx;
		ctx.m_graphicsCmds = cmdBuffer;
		ctx.m_targets = m_renderTargets.get();
		m_renderGraph->Run(ctx);

		// Transition swap chain image to format optimal for present
		auto colourBarrier = VulkanHelpers::MakeImageBarrier(swapImage,
			VK_IMAGE_ASPECT_COLOR_BIT,
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,				// src mask = wait before writing attachment
			VK_ACCESS_NONE,										// dst mask = we dont care?
			VK_IMAGE_LAYOUT_UNDEFINED,	// TODO from current swap image layout
			VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);					// to format optimal for present
		vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &colourBarrier);

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

	BufferPool* RenderSystem::GetStagingBufferPool()
	{
		return m_stagingBuffers.get();
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

	void RenderSystem::ProcessEnvironmentSettings()	// move out of render system
	{
		R3_PROF_EVENT();
		auto entities = GetSystem<Entities::EntitySystem>();
		auto w = entities->GetActiveWorld();
		if (w)
		{
			auto getClearColour = [this](const Entities::EntityHandle& e, EnvironmentSettingsComponent& cmp) {
				m_mainPassClearColour = glm::vec4(cmp.m_skyColour, 1.0f);
				return true;
			};
			Entities::Queries::ForEach<EnvironmentSettingsComponent>(w, getClearColour);
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
		if (powerState.m_isRunningOnBattery && powerState.m_batteryPercentageRemaining < 50)
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
		
		// Updates bg colour
		ProcessEnvironmentSettings();

		// Always 'render' the ImGui frame so we can begin the next one, even if we wont actually draw anything
		{
			R3_PROF_EVENT("ImGui::Render");
			ImGui::Render();
		}
		
		auto graphicsCmds = m_cmdBufferAllocator->CreateCommandBuffer(*m_device, true);
		if (!graphicsCmds)
		{
			LogError("Failed to create cmd buffer!");
			return false;
		}

		RunGraph(*m_renderGraph, graphicsCmds->m_cmdBuffer, m_swapChain->GetImages()[m_currentSwapImage], m_swapChain->GetViews()[m_currentSwapImage]);

		// submit the cmd buffer to the graphics queue
		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.pCommandBuffers = &graphicsCmds->m_cmdBuffer;	// which buffer to submit
		submitInfo.commandBufferCount = 1;

		// we describe which sync objects to wait on before execution can begin
		// and at which stage in the pipeline we should wait for them
		auto& fd = m_vk->ThisFrameData();
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

		// release the command buffer back to the pool
		m_cmdBufferAllocator->Release(*graphicsCmds);

		// present!
		// this will put the image we just rendered into the visible window.
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
		constexpr bool c_enableDynamicRendering = true;		// allows us to bypass renderpasses/subpasses
		bool enableValidationLayers = Platform::GetCmdLine().find("-debugvulkan") != std::string::npos;
		m_device = std::make_unique<Device>(m_mainWindow.get());
		return m_device->Initialise(enableValidationLayers, c_enableDynamicRendering);
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
		if (!CreateRenderGraph())
		{
			LogError("Failed to create render graph");
			return false;
		}
		m_renderTargets = std::make_unique<RenderTargetCache>();
		m_mainDeleters.PushDeleter([&]() {
			m_renderTargets = {};
		});
		m_imRenderer = std::make_unique<ImmediateRenderer>();
		if (!m_imRenderer->Initialise(*m_device, GetMainColourTargetFormat(), GetMainDepthStencilFormat()))
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
		init_info.MinImageCount = static_cast<uint32_t>(m_swapChain->GetImages().size());
		init_info.ImageCount = static_cast<uint32_t>(m_swapChain->GetImages().size());
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
			m_cmdBufferAllocator->GetPool(*m_device), m_vk->m_immediateSubmitFence, [&](VkCommandBuffer cmd) {
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