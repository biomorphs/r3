#include "imgui_system.h"
#include "event_system.h"
#include "render/render_system.h"
#include "core/log.h"
#include "core/profiler.h"
#include "core/file_io.h"
#include "external/Fork-awesome/IconsForkAwesome.h"
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

		// OpenSans as default font
		auto fontPath = FileIO::FindAbsolutePath("fonts\\Open_Sans\\static\\OpenSans-Medium.ttf");
		if (fontPath.size() > 0)
		{
			io.Fonts->AddFontFromFileTTF(fontPath.c_str(), 17);
		}

		// Fork-awesome 6 for icons
		std::string forkAwesomePath = "fonts\\Fork-awesome\\" + std::string(FONT_ICON_FILE_NAME_FK);
		static const ImWchar icons_ranges[] = { ICON_MIN_FK, ICON_MAX_FK, 0 };
		ImFontConfig icons_config; 
		icons_config.MergeMode = true; 
		icons_config.PixelSnapH = true;
		fontPath = FileIO::FindAbsolutePath(forkAwesomePath);
		if (fontPath.size() > 0)
		{
			io.Fonts->AddFontFromFileTTF(fontPath.c_str(), 17.0f, &icons_config, icons_ranges);
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