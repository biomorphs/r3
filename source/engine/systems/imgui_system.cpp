#include "imgui_system.h"
#include "event_system.h"
#include "render/render_system.h"
#include "core/log.h"
#include "core/profiler.h"
#include "core/file_io.h"
#include "engine/imgui_menubar_helper.h"
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
		const float defaultSizePx = 17.0f;
		const float largeSizePx = 22.0f;

		// OpenSans as default font
		auto mediumPath = FileIO::FindAbsolutePath("fonts\\Open_Sans\\static\\OpenSans-Medium.ttf");
		auto boldPath = FileIO::FindAbsolutePath("fonts\\Open_Sans\\static\\OpenSans-Bold.ttf");
		auto italicPath = FileIO::FindAbsolutePath("fonts\\Open_Sans\\static\\OpenSans-MediumItalic.ttf");

		// Fork-awesome 6 for icons
		auto forkAwesomePath = FileIO::FindAbsolutePath("fonts\\Fork-awesome\\" + std::string(FONT_ICON_FILE_NAME_FK));
		static const ImWchar icons_ranges[] = { ICON_MIN_FK, ICON_MAX_FK, 0 };	// this MUST be static

		ImGuiIO& io = ImGui::GetIO();
		io.Fonts->Clear();
		if (mediumPath.size() > 0)		// always load default font first
		{
			m_defaultFont = io.Fonts->AddFontFromFileTTF(mediumPath.c_str(), defaultSizePx);		
		}
		ImFontConfig icons_config;
		icons_config.MergeMode = true;	// merge fork-awesome icons into the previous (default font) only
		icons_config.PixelSnapH = true;	// align glyphs to edge
		if (forkAwesomePath.size() > 0)
		{
			io.Fonts->AddFontFromFileTTF(forkAwesomePath.c_str(), defaultSizePx, &icons_config, icons_ranges);
		}
		if (mediumPath.size() > 0)
		{
			m_largeFont = io.Fonts->AddFontFromFileTTF(mediumPath.c_str(), largeSizePx);
		}
		if (boldPath.size() > 0)
		{
			m_boldFont = io.Fonts->AddFontFromFileTTF(boldPath.c_str(), defaultSizePx);
			m_largeBoldFont = io.Fonts->AddFontFromFileTTF(boldPath.c_str(), largeSizePx);
		}
		if (italicPath.size() > 0)
		{
			m_italicFont = io.Fonts->AddFontFromFileTTF(italicPath.c_str(), defaultSizePx);
		}
	}

	void ImGuiSystem::PushDefaultFont()
	{
		ImGui::PushFont(m_defaultFont);
	}

	void ImGuiSystem::PushBoldFont()
	{
		ImGui::PushFont(m_boldFont);
	}

	void ImGuiSystem::PushItalicFont()
	{
		ImGui::PushFont(m_italicFont);
	}

	void ImGuiSystem::PushLargeFont()
	{
		ImGui::PushFont(m_largeFont);
	}

	void ImGuiSystem::PushLargeBoldFont()
	{
		ImGui::PushFont(m_largeBoldFont);
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

		// display the main menu singleton + reset state for next frame
		MenuBar::MainMenu().Display(true);
		MenuBar::MainMenu() = {};

		return true;
	}
}