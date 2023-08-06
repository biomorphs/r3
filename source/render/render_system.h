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

		bool CreateWindow();
		bool CreateVkInstance();
		bool CreatePhysicalDevice();
		bool CreateLogicalDevice();
		bool CreateSurface();
		bool CreateSwapchain();
		bool CreateRenderPass();
		bool CreateFramebuffers();
		bool CreateSimpleTriPipeline();
		bool CreateCommandPool();
		bool CreateCommandBuffer();
		bool RecordCommandBuffer(int swapImageIndex);
		bool CreateSyncObjects();
		std::unique_ptr<Window> m_mainWindow;
		struct VkStuff;
		std::unique_ptr<VkStuff> m_vk;
	};
}