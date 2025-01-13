#include "frame_scheduler_system.h"
#include "immediate_render_system.h"
#include "imgui_system.h"
#include "lights_system.h"
#include "static_mesh_system.h"
#include "static_mesh_simple_renderer.h"
#include "texture_system.h"
#include "engine/tonemap_compute.h"
#include "render/render_graph.h"
#include "render/render_system.h"
#include "core/profiler.h"
#include "core/log.h"
#include <imgui.h>

namespace R3
{
	FrameScheduler::FrameScheduler()
	{
	}

	FrameScheduler::~FrameScheduler()
	{
	}

	void FrameScheduler::RegisterTickFns()
	{
		Systems::GetInstance().RegisterTick("FrameScheduler::BuildRenderGraph", [this]() {
			return BuildRenderGraph();
		});
		Systems::GetInstance().RegisterTick("FrameScheduler::ShowGui", [this]() {
			return ShowGui();
		});
	}

	bool FrameScheduler::Init()
	{
		m_tonemapComputeRenderer = std::make_unique<TonemapCompute>();
		GetSystem<RenderSystem>()->m_onShutdownCbs.AddCallback([this](Device& d) {
			m_tonemapComputeRenderer->Cleanup(d);
		});
		return true;
	}

	std::unique_ptr<DrawPass> FrameScheduler::MakeMainPass(const RenderTargetInfo& mainColour, const RenderTargetInfo& mainDepth)
	{
		R3_PROF_EVENT();
		auto mainPass = std::make_unique<DrawPass>();	
		mainPass->m_name = "Main Pass";
		mainPass->m_colourAttachments.push_back({ mainColour, DrawPass::AttachmentLoadOp::Clear });
		mainPass->m_depthAttachment = { mainDepth, DrawPass::AttachmentLoadOp::Clear };
		mainPass->m_getExtentsFn = []() -> glm::vec2 {
			return GetSystem<RenderSystem>()->GetWindowExtents();
		};
		mainPass->m_getClearColourFn = []() -> glm::vec4 {
			return glm::vec4(GetSystem<LightsSystem>()->GetSkyColour(), 1.0f);
		};
		mainPass->m_onBegin.AddCallback([](RenderPassContext& ctx) {
			R3_PROF_GPU_EVENT("Main Pass Begin");
			GetSystem<LightsSystem>()->CollectLightsForDrawing(ctx);
			GetSystem<TextureSystem>()->ProcessLoadedTextures(ctx);
			GetSystem<StaticMeshSystem>()->OnMainPassBegin(ctx);
			GetSystem<StaticMeshSimpleRenderer>()->OnMainPassBegin(ctx);
			GetSystem<ImmediateRenderSystem>()->OnMainPassBegin(ctx);
		});
		mainPass->m_onDraw.AddCallback([](RenderPassContext& ctx) {
			R3_PROF_GPU_EVENT("Main Pass");
			GetSystem<StaticMeshSimpleRenderer>()->OnMainPassDraw(ctx);
			GetSystem<ImmediateRenderSystem>()->OnMainPassDraw(ctx);
		});
		mainPass->m_onEnd.AddCallback([](RenderPassContext& ctx) {
			GetSystem<StaticMeshSimpleRenderer>()->OnMainPassEnd(ctx);
		});
		return mainPass;
	}

	std::unique_ptr<TransferPass> FrameScheduler::MakeColourBlitToPass(std::string_view name, const RenderTargetInfo& srcTarget, const RenderTargetInfo& destTarget)
	{
		R3_PROF_EVENT();
		auto blitPass = std::make_unique<TransferPass>();		// blit LDR colour to swap chain
		blitPass->m_name = name;
		blitPass->m_inputs.push_back(srcTarget);
		blitPass->m_outputs.push_back(destTarget);
		blitPass->m_onRun.AddCallback([this, srcTarget, destTarget](RenderPassContext& ctx) {
			R3_PROF_GPU_EVENT("Main Pass blit to swap");
			glm::vec2 srcSize = ctx.m_targets->GetTargetSize(srcTarget);
			glm::vec2 dstSize = ctx.m_targets->GetTargetSize(destTarget);
			glm::vec2 srcExtents = glm::min(srcSize, dstSize);	// clip blit dimensions to size of dest target
			VkExtent2D extents((uint32_t)srcExtents.x, (uint32_t)srcExtents.y);
			VulkanHelpers::BlitColourImageToImage(ctx.m_graphicsCmds,
				ctx.GetResolvedTarget(srcTarget)->m_image, extents,
				ctx.GetResolvedTarget(destTarget)->m_image, extents);
		});
		return blitPass;
	}

	std::unique_ptr<DrawPass> FrameScheduler::MakeImguiPass(const RenderTargetInfo& colourTarget)
	{
		R3_PROF_EVENT();
		auto render = GetSystem<RenderSystem>();
		auto imgui = GetSystem<ImGuiSystem>();
		auto imguiPass = std::make_unique<DrawPass>();			// imgui draw direct to swap image
		imguiPass->m_name = "ImGUI Render";
		imguiPass->m_colourAttachments.push_back({ colourTarget, DrawPass::AttachmentLoadOp::Load });
		imguiPass->m_getExtentsFn = [render]() -> glm::vec2 {
			return render->GetWindowExtents();
		};
		imguiPass->m_onDraw.AddCallback([imgui](RenderPassContext& ctx) {
			R3_PROF_GPU_EVENT("IMGUI");
			imgui->OnDraw(ctx);
		});
		return imguiPass;
	}

	std::unique_ptr<ComputeDrawPass> FrameScheduler::MakeTonemapToLDRPass(const RenderTargetInfo& mainColour, const RenderTargetInfo& mainColourLDR)
	{
		// This pass reads the main colour image, runs tonemap operator and writes to the swap chain image
		auto tonemapPass = std::make_unique<ComputeDrawPass>();
		tonemapPass->m_name = "Tonemap Compute";
		tonemapPass->m_inputColourAttachments.push_back(mainColour);	// HDR input
		tonemapPass->m_outputColourAttachments.push_back(mainColourLDR);	// output to LDR image
		tonemapPass->m_onRun.AddCallback([this, mainColour, mainColourLDR](RenderPassContext& ctx) {
			auto inTarget = ctx.GetResolvedTarget(mainColour);
			auto inSize = ctx.m_targets->GetTargetSize(inTarget->m_info);
			auto outTarget = ctx.GetResolvedTarget(mainColourLDR);
			auto outSize = ctx.m_targets->GetTargetSize(outTarget->m_info);
			m_tonemapComputeRenderer->Run(*ctx.m_device, ctx.m_graphicsCmds, *inTarget, inSize, *outTarget, outSize);
		});
		return tonemapPass;
	}

	// This describes the entire renderer as a series of passes that run in series
	bool FrameScheduler::BuildRenderGraph()
	{
		R3_PROF_EVENT();		
		auto render = GetSystem<RenderSystem>();
		m_allCurrentTargets.clear();

		RenderTargetInfo swapchainImage("Swapchain");	// Swap chain image, this will be presented to the screen each frame
		RenderTargetInfo mainColour("MainColour");		// HDR colour buffer, linear encoding
		mainColour.m_format = VK_FORMAT_R16G16B16A16_SFLOAT;
		mainColour.m_usageFlags = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
		m_allCurrentTargets.push_back(mainColour);

		RenderTargetInfo mainDepth("MainDepth");		// Main depth buffer
		mainDepth.m_format = VK_FORMAT_D32_SFLOAT;		// may be overkill
		mainDepth.m_usageFlags = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
		mainDepth.m_aspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT;
		m_allCurrentTargets.push_back(mainDepth);

		RenderTargetInfo mainColourLDR("MainColourLDR");		// LDR colour buffer after tonemapping
		mainColourLDR.m_format = VK_FORMAT_R16G16B16A16_SFLOAT;	// still using floating point storage
		mainColourLDR.m_usageFlags = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
		m_allCurrentTargets.push_back(mainColourLDR);

		auto& graph = render->GetRenderGraph();
		graph.m_allPasses.clear();
		graph.m_allPasses.push_back(MakeMainPass(mainColour, mainDepth));	// main render (HDR)
		graph.m_allPasses.push_back(MakeTonemapToLDRPass(mainColour, mainColourLDR));	// HDR -> LDR
		graph.m_allPasses.push_back(MakeColourBlitToPass("LDR to swap", mainColourLDR, swapchainImage));	// blit LDR -> swap
		if (m_colourTargetDebuggerEnabled && m_colourDebugTargetName.length() > 0)
		{
			auto srcTarget = std::find_if(m_allCurrentTargets.begin(), m_allCurrentTargets.end(), [&](const RenderTargetInfo& info) {
				return info.m_name == m_colourDebugTargetName;
			});
			if (srcTarget != m_allCurrentTargets.end())
			{
				graph.m_allPasses.push_back(MakeColourBlitToPass("Colour target debug", *srcTarget, swapchainImage));	// blit debug target -> swap
			}
		}
		graph.m_allPasses.push_back(MakeImguiPass(swapchainImage));		// imgui to swap
		
		return true;
	}

	bool FrameScheduler::ShowGui()
	{
		ImGui::Begin("Render target visualiser");
		if (ImGui::BeginCombo("Colour Target", m_colourDebugTargetName.c_str()))
		{
			for (int target = 0; target < m_allCurrentTargets.size(); ++target)
			{
				if (m_allCurrentTargets[target].m_aspectFlags == VK_IMAGE_ASPECT_COLOR_BIT)
				{
					bool selected = (m_colourDebugTargetName == m_allCurrentTargets[target].m_name);
					if (ImGui::Selectable(m_allCurrentTargets[target].m_name.c_str(), selected))
					{
						m_colourDebugTargetName = m_allCurrentTargets[target].m_name;
					}
					if (selected)
					{
						ImGui::SetItemDefaultFocus();	// ensure keyboard/controller navigation works
					}
				}
			}
			ImGui::EndCombo();
		}
		ImGui::End();
		return true;
	}
}