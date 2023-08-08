#pragma once
#include "engine/systems.h"

struct VkExtensionProperties;
namespace R3
{
	class Window;
	class RenderSystem : public System
	{
	public:
		static std::string_view GetName() { return "Render"; }
		RenderSystem();
		virtual ~RenderSystem();
		virtual void RegisterTickFns();
		virtual bool Init();
		virtual void Shutdown();
	private:
		bool DrawFrame();
		bool InitialiseVMA();
		bool CreateWindow();
		bool CreateVkInstance();
		bool CreatePhysicalDevice();
		bool CreateLogicalDevice();
		bool CreateSurface();
		bool CreateSwapchain();
		bool CreateRenderPass();
		bool CreateFramebuffers();
		bool CreateSimpleTriPipelines();
		bool CreateCommandPool();
		bool CreateCommandBuffers();
		bool RecordCommandBuffer(int swapImageIndex);
		bool CreateSyncObjects();
		void DestroySwapchain();
		bool RecreateSwapchainAndFramebuffers();
		void OnSystemEvent(void* ev);

		bool m_isWindowMinimised = false;
		bool m_recreateSwapchain = false;
		std::unique_ptr<Window> m_mainWindow;
		struct VkStuff;
		std::unique_ptr<VkStuff> m_vk;
	};
}