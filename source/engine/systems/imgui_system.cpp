#include "imgui_system.h"
#include "event_system.h"
#include "render/render_system.h"
#include "core/log.h"
#include "core/profiler.h"
#include "core/file_io.h"
#include <imgui.h>
#include <imgui_impl_sdl2.h>

namespace R3
{
	void ImGuiSystem::RegisterTickFns()
	{
		R3_PROF_EVENT();
		RegisterTick("ImGui::FrameStart", [this]() {
			return OnFrameStart();
		});
	}

	void ImGuiSystem::OnSystemEvent(void* e)
	{
		R3_PROF_EVENT();
		ImGui_ImplSDL2_ProcessEvent(static_cast<SDL_Event*>(e));
	}

	void ImGuiSystem::LoadFonts()
	{
		ImGuiIO& io = ImGui::GetIO();
		io.Fonts->Clear();
		//io.Fonts->AddFontDefault();

		std::vector<uint8_t> ttfBuffer;
		if (FileIO::LoadBinaryFile("fonts\\Open_Sans\\OpenSans-VariableFont_wdth,wght.ttf", ttfBuffer))
		{
			io.Fonts->AddFontFromMemoryTTF(ttfBuffer.data(), static_cast<int>(ttfBuffer.size()), 17);
		}
	}

	bool ImGuiSystem::OnFrameStart()
	{
		R3_PROF_EVENT();
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