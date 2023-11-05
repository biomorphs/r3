#pragma once
#include "engine/systems.h"
#include "core/glm_headers.h"

namespace R3
{
	class Device;
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
		glm::vec2 GetWindowExtents();

		// Called from imgui system
		bool InitImGui();
		void ImGuiNewFrame();
	private:
		void DrawImgui(int swapImageIndex);
		void ProcessEnvironmentSettings();
		bool ShowGui();
		bool CreateMesh();
		bool DrawFrame();
		bool CreateWindow();
		bool CreateDepthBuffer();
		bool CreateSimpleTriPipelines();
		bool CreateCommandPools();
		bool CreateCommandBuffers();
		bool RecordCommandBuffer(int swapImageIndex);
		bool CreateSyncObjects();
		bool RecreateSwapchain();
		void OnSystemEvent(void* ev);
		bool CreateDevice();
		bool m_isWindowMinimised = false;
		bool m_recreateSwapchain = false;
		std::unique_ptr<Window> m_mainWindow;
		std::unique_ptr<Device> m_device;
		struct VkStuff;
		std::unique_ptr<VkStuff> m_vk;
		glm::vec4 m_mainPassClearColour = { 0,0,0,1 };
	};
}