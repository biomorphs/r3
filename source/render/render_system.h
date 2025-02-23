#pragma once
#include "engine/systems.h"
#include "core/callback_array.h"
#include "core/glm_headers.h"
#include "deletion_queue.h"

struct VkCommandBuffer_T;
struct VkImageView_T;
struct VkImage_T;
enum VkFormat;
namespace R3
{
	class RenderTargetCache;
	class RenderGraph;
	class Device;
	class Window;
	class Swapchain;
	class ImmediateRenderer;
	class BufferPool;
	class CommandBufferAllocator;
	struct RenderTargetInfo;
	class RenderSystem : public System
	{
	public:
		static std::string_view GetName() { return "Render"; }
		RenderSystem();
		virtual ~RenderSystem();
		virtual void RegisterTickFns();
		virtual bool Init();
		virtual void Shutdown();

		glm::vec2 GetWindowExtents();
		Device* GetDevice();
		Window* GetMainWindow();
		Swapchain* GetSwapchain();
		BufferPool* GetBufferPool();
		CommandBufferAllocator* GetCommandBufferAllocator();
		RenderGraph& GetRenderGraph() { return *m_renderGraph; }
		RenderTargetInfo GetSwapchainTargetInfo();		// required when render graph nodes need to use the swap chain
		RenderTargetCache* GetRenderTargetCache();

		// Run some graphics code immediately
		void RunGraphicsCommandsImmediate(std::function<void(VkCommandBuffer_T*)> fn);

		// Callbacks (other systems can hook into renderer here)
		// public out of reasons of laziness (no point writing a bunch of register/unregister)
		// careful with thread safety when adding new ones
		using ShutdownCallback = std::function<void(Device&)>;		// called when shutting down renderer
		CallbackArray<ShutdownCallback> m_onShutdownCbs;

		// called from render stats system
		void ShowGpuProfilerGui();

	private:
		void RunGraph(RenderGraph& r, VkCommandBuffer_T* cmdBuffer, VkImage_T* swapImage, VkImageView_T* swapImageView);
		bool AcquireSwapImage();
		bool ShowGui();
		bool DrawFrame();
		bool CreateWindow();
		bool CreateSyncObjects();
		bool CreateTimestampHandlers();
		bool RecreateSwapchain();
		bool PrepareSwapchain();
		void OnSystemEvent(void* ev);
		bool CreateDevice();
		bool CreateSwapchain();
		bool ShouldEnableVsync();
		bool m_isWindowMinimised = false;
		bool m_recreateSwapchain = false;
		std::unique_ptr<RenderTargetCache> m_renderTargets;
		std::unique_ptr<RenderGraph> m_renderGraph;
		std::unique_ptr<Window> m_mainWindow;
		std::unique_ptr<Device> m_device;
		std::unique_ptr<Swapchain> m_swapChain;
		std::unique_ptr<BufferPool> m_bufferPool;
		std::unique_ptr<CommandBufferAllocator> m_cmdBufferAllocator;
		struct VkStuff;
		std::unique_ptr<VkStuff> m_vk;
		uint32_t m_currentSwapImage = -1;
		DeletionQueue m_mainDeleters;	// only ran when the render system shuts down
	};
}