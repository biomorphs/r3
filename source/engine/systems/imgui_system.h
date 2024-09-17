#pragma once
#include "engine/systems.h"

struct ImFont;
struct VkDescriptorPool_T;
namespace R3
{
	class ImGuiSystem : public System
	{
	public:
		static std::string_view GetName() { return "ImGui"; }
		virtual void RegisterTickFns();
		virtual bool Init();
		void OnDraw(class RenderPassContext&);
		void PushDefaultFont();		// call ImGui::PopFont(); when finished
		void PushBoldFont();		// call ImGui::PopFont(); when finished
		void PushItalicFont();		// call ImGui::PopFont(); when finished
		void PushLargeFont();		// call ImGui::PopFont(); when finished
		void PushLargeBoldFont();	// call ImGui::PopFont(); when finished
	private:
		void CreateScriptBindings();
		void LoadFonts();
		void OnShutdown(class Device&);
		void OnSystemEvent(void*);
		bool OnFrameStart();
		bool m_initialised = false;
		ImFont* m_defaultFont = nullptr;
		ImFont* m_boldFont = nullptr;
		ImFont* m_italicFont = nullptr;
		ImFont* m_largeFont = nullptr;
		ImFont* m_largeBoldFont = nullptr;
		VkDescriptorPool_T* m_descriptorPool = nullptr;
	};
}