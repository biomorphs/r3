#include "imgui_system.h"
#include "event_system.h"
#include "render/render_system.h"
#include "core/log.h"
#include <imgui.h>
#include <imgui_impl_sdl2.h>

namespace R3
{
	void ImGuiSystem::RegisterTickFns()
	{
		RegisterTick("ImGui::FrameStart", [this]() {
			return OnFrameStart();
		});
	}

	void ImGuiSystem::OnSystemEvent(void* e)
	{
		ImGui_ImplSDL2_ProcessEvent(static_cast<SDL_Event*>(e));
	}

	bool ImGuiSystem::OnFrameStart()
	{
		auto r = GetSystem<RenderSystem>();
		if (!m_initialised)
		{
			ImGui::CreateContext();	//this initializes the core structures of imgui
			if (!r->InitImGui())	// renderer deals with drawing
			{
				LogError("Failed to initialise ImGui rendering");
				return false;
			}
			// we will pass the SDL events to imgui
			GetSystem<EventSystem>()->RegisterEventHandler([this](void* ev) {
				OnSystemEvent(ev);
			});
			m_initialised = true;
		}

		r->ImGuiNewFrame();

		return true;
	}
}