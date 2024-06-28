#pragma once
#include "engine/systems.h"
#include "engine/callback_array.h"
#include "core/glm_headers.h"
#include "deletion_queue.h"

struct VkCommandBuffer_T;
struct VkImageView_T;
struct VkImage_T;
struct VkExtent2D;
enum VkFormat;
namespace R3
{
	class RenderGraph;
	class Device;
	class Window;
	class Swapchain;
	class ImmediateRenderer;
	class BufferPool;
	class CommandBufferAllocator;
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
		RenderGraph& GetRenderGraph() { return *m_renderGraph; }
		ImmediateRenderer& GetImRenderer() { return *m_imRenderer; }
		VkFormat GetMainColourTargetFormat();
		VkFormat GetMainDepthStencilFormat();
		Device* GetDevice();
		BufferPool* GetStagingBufferPool();
		CommandBufferAllocator* GetCommandBufferAllocator();

		// Called from imgui system
		bool InitImGui();
		void ImGuiNewFrame();

		// Callbacks (other systems hook into renderer here)
		// public out of reasons of laziness (no point writing a bunch of register/unregister)
		// careful with thread safety when adding new ones
		using ShutdownCallback = std::function<void(Device&)>;							// called when shutting down renderer
		using MainPassBeginCallback = std::function<void(Device&, VkCommandBuffer_T*)>;	// called before vkCmdBeginRendering
		using MainPassDrawCallback = std::function<void(Device&, VkCommandBuffer_T*, const VkExtent2D&)>;	// draw stuff to main pass here
		using MainPassEndCallback = std::function<void(Device&)>;						// called after vkCmdEndRendering
		CallbackArray<MainPassBeginCallback> m_onMainPassBegin;
		CallbackArray<MainPassDrawCallback> m_onMainPassDraw;
		CallbackArray<MainPassEndCallback> m_onMainPassEnd;
		CallbackArray<ShutdownCallback> m_onShutdownCbs;

	private:
		bool AcquireSwapImage();
		void RecordMainPass(VkCommandBuffer_T* cmdBuffer);
		void DrawImgui(VkImageView_T* targetView, VkCommandBuffer_T* cmdBuffer);
		bool RecordCommandBuffer(VkImage_T* swapImage, VkImageView_T* swapImageView, VkCommandBuffer_T* cmdBuffer);
		void ProcessEnvironmentSettings();
		bool ShowGui();
		bool DrawFrame();
		bool CreateWindow();
		bool CreateDepthBuffer();
		bool CreateBackBuffer();
		bool CreateSyncObjects();
		bool RecreateSwapchain();
		bool PrepareSwapchain();
		void OnSystemEvent(void* ev);
		bool CreateDevice();
		bool CreateSwapchain();
		bool ShouldEnableVsync();
		bool m_isWindowMinimised = false;
		bool m_recreateSwapchain = false;
		std::unique_ptr<RenderGraph> m_renderGraph;
		std::unique_ptr<Window> m_mainWindow;
		std::unique_ptr<Device> m_device;
		std::unique_ptr<Swapchain> m_swapChain;
		std::unique_ptr<ImmediateRenderer> m_imRenderer;
		std::unique_ptr<BufferPool> m_stagingBuffers;
		std::unique_ptr<CommandBufferAllocator> m_cmdBufferAllocator;
		struct VkStuff;
		std::unique_ptr<VkStuff> m_vk;
		glm::vec4 m_mainPassClearColour = { 0,0,0,1 };
		uint32_t m_currentSwapImage = -1;
		DeletionQueue m_mainDeleters;	// only ran when the render system shuts down
	};
}