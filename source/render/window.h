#pragma once

#include "core/glm_headers.h"
#include <string>

struct SDL_Window;
namespace R3
{
	class Window
	{
	public:
		enum CreationFlags
		{
			CreateFullscreen = (1 << 1),
			CreateFullscreenDesktop = (1 << 2),
			CreateResizable = (1 << 3),
			CreateMinimized = (1 << 4),
			CreateMaximized = (1 << 5)
		};
		struct Properties
		{
			std::string m_title = "New Window";
			int m_sizeX = -1;
			int m_sizeY = -1;
			int m_flags = 0;
		};

		Window(const Properties& properties);
		~Window();

		void Show();
		void Hide();
		glm::ivec2 GetSize() const { return { m_properties.m_sizeX, m_properties .m_sizeY}; }

		SDL_Window* GetHandle();
		inline const Properties& GetProperties() { return m_properties; }

	private:
		SDL_Window* m_handle = nullptr;
		Properties m_properties;
	};
}