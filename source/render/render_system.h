#pragma once
#include "engine/systems.h"

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
		std::unique_ptr<Window> m_mainWindow;
	};
}