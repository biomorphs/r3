#include "window.h"
#include "core/log.h"
#include "core/profiler.h"
#include <SDL.h>

namespace R3
{
	Window::Window(const Properties& props)
		: m_properties(props)
	{
		R3_PROF_EVENT();
		int windowPosX = SDL_WINDOWPOS_UNDEFINED;
		int windowPosY = SDL_WINDOWPOS_UNDEFINED;
		int windowFlags = SDL_WINDOW_HIDDEN | SDL_WINDOW_VULKAN | SDL_WINDOW_ALLOW_HIGHDPI;
		
		if (props.m_flags & CreateFullscreen)
			windowFlags |= SDL_WINDOW_FULLSCREEN;
		else if (props.m_flags & CreateFullscreenDesktop)
			windowFlags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
		else
		{
			if (props.m_flags & CreateResizable)
				windowFlags |= SDL_WINDOW_RESIZABLE;

			if (props.m_flags & CreateMinimized)
				windowFlags |= SDL_WINDOW_MINIMIZED;
			else if (props.m_flags & CreateResizable)
				windowFlags |= SDL_WINDOW_MAXIMIZED;
		}

		m_handle = SDL_CreateWindow(props.m_title.c_str(), windowPosX, windowPosY, props.m_sizeX, props.m_sizeY, windowFlags);
		if (m_handle == nullptr)
		{
			LogError("Error creating window ({})", SDL_GetError());
		}
		assert(m_handle);
	}

	Window::~Window()
	{
		SDL_DestroyWindow(m_handle);
	}

	void Window::Show()
	{
		assert(m_handle);
		SDL_ShowWindow(m_handle);
	}

	void Window::Hide()
	{
		assert(m_handle);
		SDL_HideWindow(m_handle);
	}

	SDL_Window* Window::GetHandle() 
	{
		return m_handle;
	}
}