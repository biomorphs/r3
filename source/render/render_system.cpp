#include "render_system.h"
#include "window.h"

namespace R3
{
	RenderSystem::RenderSystem()
	{
	}

	RenderSystem::~RenderSystem()
	{
	}

	void RenderSystem::RegisterTickFns()
	{
	}

	bool RenderSystem::Init()
	{
		Window::Properties windowProps;
		windowProps.m_sizeX = 1280;
		windowProps.m_sizeY = 720;
		windowProps.m_title = "R3";
		windowProps.m_flags = 0;
		m_mainWindow = std::make_unique<Window>(windowProps);
		m_mainWindow->Show();
		return true;
	}

	void RenderSystem::Shutdown()
	{
		m_mainWindow = nullptr;
	}
}