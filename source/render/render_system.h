#pragma once
#include "engine/systems.h"
#include "core/glm_headers.h"

struct VkViewport;
struct VkRect2D;
namespace R3
{
	class Device;
	class Window;
	class Swapchain;
	class ImmediateRenderer;
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

		// Called from imgui system
		bool InitImGui();
		void ImGuiNewFrame();
	private:
		void RecordMainPass(int swapImageIndex);
		void DrawImgui(int swapImageIndex);
		void ProcessEnvironmentSettings();
		bool ShowGui();
		bool DrawFrame();
		bool CreateWindow();
		bool CreateDepthBuffer();
		bool CreateCommandPools();
		bool CreateCommandBuffers();
		bool RecordCommandBuffer(int swapImageIndex);
		bool CreateSyncObjects();
		bool RecreateSwapchain();
		bool PrepareSwapchain();
		void OnSystemEvent(void* ev);
		bool CreateDevice();
		bool CreateSwapchain();
		bool m_isWindowMinimised = false;
		bool m_recreateSwapchain = false;
		std::unique_ptr<Window> m_mainWindow;
		std::unique_ptr<Device> m_device;
		std::unique_ptr<Swapchain> m_swapChain;
		std::unique_ptr<ImmediateRenderer> m_imRenderer;
		struct VkStuff;
		std::unique_ptr<VkStuff> m_vk;
		glm::vec4 m_mainPassClearColour = { 0,0,0,1 };
	};
}