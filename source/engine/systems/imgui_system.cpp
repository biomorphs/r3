#include "imgui_system.h"
#include "event_system.h"
#include "lua_system.h"
#include "render/render_system.h"
#include "render/vulkan_helpers.h"
#include "render/device.h"
#include "render/window.h"
#include "render/swap_chain.h"
#include "render/render_graph.h"
#include "core/log.h"
#include "core/profiler.h"
#include "core/file_io.h"
#include "engine/imgui_menubar_helper.h"
#include "external/Fork-awesome/IconsForkAwesome.h"
#include <imgui.h>
#include <imgui_impl_vulkan.h>
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

	void ImGuiSystem::CreateScriptBindings()
	{
		R3_PROF_EVENT();
		auto lua = GetSystem<LuaSystem>();
		const auto c_namespace = "ImGui";
		lua->RegisterFunction("Begin", [](std::string_view titleText, bool keepOpen) {
			bool actualKeepOpen = keepOpen;
			ImGui::Begin(titleText.data(), &actualKeepOpen);
			return actualKeepOpen;
		}, c_namespace);
		lua->RegisterFunction("End", []() {
			ImGui::End();
		}, c_namespace);
		lua->RegisterFunction("BeginTooltip", []() {
			return ImGui::BeginTooltip();
		}, c_namespace);
		lua->RegisterFunction("EndTooltip", []() {
			ImGui::EndTooltip();
		}, c_namespace);
		lua->RegisterFunction("Text", [](std::string_view txt) {
			ImGui::Text(txt.data());
		}, c_namespace);
		lua->RegisterFunction("TextColoured", [](glm::vec4 colour, std::string_view txt) {
			ImVec4 imVec4(colour.r, colour.g, colour.b, colour.a);
			ImGui::TextColored(imVec4, txt.data());
		}, c_namespace);
		lua->RegisterFunction("SeparatorText", [](std::string_view txt) {
			ImGui::SeparatorText(txt.data());
		}, c_namespace);
		lua->RegisterFunction("Separator", []() {
			ImGui::Separator();
		}, c_namespace);
		lua->RegisterFunction("NewLine", []() {
			ImGui::NewLine();
		}, c_namespace);
		lua->RegisterFunction("SameLine", []() {
			ImGui::SameLine();
		}, c_namespace);
		lua->RegisterFunction("PushDefaultFont", [this]() {
			PushDefaultFont();
		}, c_namespace);
		lua->RegisterFunction("PushBoldFont", [this]() {
			PushBoldFont();
		}, c_namespace);
		lua->RegisterFunction("PushItalicFont", [this]() {
			PushItalicFont();
		}, c_namespace);
		lua->RegisterFunction("PushLargeFont", [this]() {
			PushLargeFont();
		}, c_namespace);
		lua->RegisterFunction("PushLargeBoldFont", [this]() {
			PushLargeBoldFont();
		}, c_namespace);
		lua->RegisterFunction("PopFont", []() {
			ImGui::PopFont();
		}, c_namespace);
		lua->RegisterFunction("Button", [](std::string_view txt) {
			return ImGui::Button(txt.data());
		}, c_namespace);
		lua->RegisterFunction("Checkbox", [](std::string_view txt, bool value) {
			ImGui::Checkbox(txt.data(), &value);
			return value;
		}, c_namespace);
		lua->RegisterFunction("ProgressBar", [](std::string_view txt, float value, float maxValue, glm::vec2 size) {
			ImVec2 imSize(size.x, size.y);
			ImGui::ProgressBar(value / (maxValue+ 0.000001f), imSize, txt.data());
		}, c_namespace);
		lua->RegisterFunction("TreeNode", [](std::string_view txt) {
			return ImGui::TreeNode(txt.data());
		}, c_namespace);
		lua->RegisterFunction("TreePop", []() {
			return ImGui::TreePop();
		}, c_namespace);
		lua->RegisterFunction("InputFloat", [](std::string_view txt, float* value, float step) {
			return ImGui::InputFloat(txt.data(), value, step);
		}, c_namespace);
		lua->RegisterFunction("InputInt", [](std::string_view txt, int* value, int step) {
			return ImGui::InputInt(txt.data(), value, step);
		}, c_namespace);
	}

	bool ImGuiSystem::Init()
	{
		R3_PROF_EVENT();
		auto r = GetSystem<RenderSystem>();
		VkDescriptorPoolSize pool_sizes[] =		// this is way bigger than it needs to be!
		{
			{ VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
			{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
			{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
			{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
		};
		VkDescriptorPoolCreateInfo pool_info = {};
		pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;	// allows us to free individual descriptor sets
		pool_info.maxSets = 1000;												// way bigger than needed!
		pool_info.poolSizeCount = static_cast<uint32_t>(std::size(pool_sizes));
		pool_info.pPoolSizes = pool_sizes;
		if (!VulkanHelpers::CheckResult(vkCreateDescriptorPool(r->GetDevice()->GetVkDevice(), &pool_info, nullptr, &m_descriptorPool)))
		{
			LogError("Failed to create descriptor pool for imgui");
			return false;
		}

		ImGui::CreateContext();	//this initializes the core structures of imgui

		// initialise imgui for SDL
		if (!ImGui_ImplSDL2_InitForVulkan( r->GetMainWindow()->GetHandle()))
		{
			LogError("Failed to init imgui for SDL/Vulkan");
			return false;
		}

		// this initializes imgui for Vulkan
		auto swapChain = r->GetSwapchain();
		ImGui_ImplVulkan_InitInfo init_info = { 0 };
		init_info.Instance = r->GetDevice()->GetVkInstance();
		init_info.PhysicalDevice = r->GetDevice()->GetPhysicalDevice().m_device;
		init_info.Device = r->GetDevice()->GetVkDevice();
		init_info.Queue = r->GetDevice()->GetGraphicsQueue();
		init_info.DescriptorPool = m_descriptorPool;
		init_info.MinImageCount = static_cast<uint32_t>(swapChain->GetImages().size());
		init_info.ImageCount = static_cast<uint32_t>(swapChain->GetImages().size());
		init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
		init_info.UseDynamicRendering = true;
		init_info.PipelineRenderingCreateInfo = {};
		init_info.PipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
		init_info.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
		init_info.PipelineRenderingCreateInfo.pColorAttachmentFormats = &swapChain->GetFormat().format;

		if (!ImGui_ImplVulkan_Init(&init_info))
		{
			LogError("Failed to init imgui for Vulkan");
			return false;
		}

		LoadFonts();

		// upload the font texture straight away
		r->RunGraphicsCommandsImmediate([&](VkCommandBuffer cmd) {
			ImGui_ImplVulkan_CreateFontsTexture();
		});
		ImGui_ImplVulkan_DestroyFontsTexture();	// destroy the font data once it is uploaded

		r->m_onShutdownCbs.AddCallback([this](Device& d) {
			OnShutdown(d);
		});

		// we will pass the SDL events to imgui
		GetSystem<EventSystem>()->RegisterEventHandler([this](void* ev) {
			OnSystemEvent(ev);
		});

		CreateScriptBindings();

		return true;
	}

	void ImGuiSystem::OnDraw(RenderPassContext& ctx)
	{
		R3_PROF_EVENT();
		ImGui::Render();
		ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), ctx.m_graphicsCmds);
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

		ImGui_ImplVulkan_NewFrame();
		ImGui_ImplSDL2_NewFrame();
		ImGui::NewFrame();

		// display the main menu singleton + reset state for next frame
		MenuBar::MainMenu().Display(true);
		MenuBar::MainMenu() = {};

		return true;
	}

	void ImGuiSystem::OnShutdown(Device& d)
	{
		R3_PROF_EVENT();

		ImGui_ImplVulkan_Shutdown();
		vkDestroyDescriptorPool(d.GetVkDevice(), m_descriptorPool, nullptr);
	}
}