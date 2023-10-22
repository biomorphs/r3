#pragma once
#include "engine/systems.h"

namespace R3
{
	class ImGuiSystem : public System
	{
	public:
		static std::string_view GetName() { return "ImGui"; }
		virtual void RegisterTickFns();
		void LoadFonts();	// called by render system when imgui is initialised
	private:
		void OnSystemEvent(void*);
		bool OnFrameStart();
		bool m_initialised = false;
	};
}