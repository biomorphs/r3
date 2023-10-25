#pragma once
#include "engine/systems.h"

struct ImFont;
namespace R3
{
	class ImGuiSystem : public System
	{
	public:
		static std::string_view GetName() { return "ImGui"; }
		virtual void RegisterTickFns();
		void LoadFonts();	// called by render system when imgui is initialised
		void PushDefaultFont();		// call ImGui::PopFont(); when finished
		void PushBoldFont();		// call ImGui::PopFont(); when finished
		void PushItalicFont();		// call ImGui::PopFont(); when finished
		void PushLargeFont();		// call ImGui::PopFont(); when finished
	private:
		void OnSystemEvent(void*);
		bool OnFrameStart();
		bool m_initialised = false;
		ImFont* m_defaultFont = nullptr;
		ImFont* m_boldFont = nullptr;
		ImFont* m_italicFont = nullptr;
		ImFont* m_largeFont = nullptr;
	};
}