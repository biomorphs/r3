#include "frame_scheduler_system.h"
#include "immediate_render_system.h"
#include "imgui_system.h"
#include "lights_system.h"
#include "static_mesh_system.h"
#include "static_mesh_simple_renderer.h"
#include "texture_system.h"
#include "render/render_graph.h"
#include "render/render_system.h"
#include "core/profiler.h"
#include "core/log.h"

namespace R3
{
	void FrameScheduler::RegisterTickFns()
	{
		Systems::GetInstance().RegisterTick("FrameScheduler::BuildRenderGraph", [this]() {
			return BuildRenderGraph();
		});
	}

	VkFormat FrameScheduler::GetMainColourTargetFormat()
	{
		return VK_FORMAT_R16G16B16A16_SFLOAT;	// HDR-lite
	}

	VkFormat FrameScheduler::GetMainDepthStencilFormat()
	{
		return VK_FORMAT_D32_SFLOAT;			// Probably too fat
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
			return GetSystem<StaticMeshSimpleRenderer>()->GetMainColourClearValue();
		};
		mainPass->m_onBegin.AddCallback([](RenderPassContext& ctx) {
			GetSystem<LightsSystem>()->CollectLightsForDrawing(ctx);
			GetSystem<TextureSystem>()->ProcessLoadedTextures(ctx);
			GetSystem<StaticMeshSystem>()->OnMainPassBegin(ctx);
			GetSystem<StaticMeshSimpleRenderer>()->OnMainPassBegin(ctx);
			GetSystem<ImmediateRenderSystem>()->OnMainPassBegin(ctx);
		});
		mainPass->m_onDraw.AddCallback([](RenderPassContext& ctx) {
			GetSystem<StaticMeshSimpleRenderer>()->OnMainPassDraw(ctx);
			GetSystem<ImmediateRenderSystem>()->OnMainPassDraw(ctx);
		});
		mainPass->m_onEnd.AddCallback([](RenderPassContext& ctx) {
			GetSystem<ImmediateRenderSystem>()->OnMainPassEnd(ctx);
		});
		return mainPass;
	}

	std::unique_ptr<TransferPass> FrameScheduler::MakeMainBlitToSwapPass(const RenderTargetInfo& mainColour, const RenderTargetInfo& swapchain)
	{
		R3_PROF_EVENT();
		auto blitPass = std::make_unique<TransferPass>();		// blit HDR colour to swap chain
		blitPass->m_inputs.push_back(mainColour);
		blitPass->m_outputs.push_back(swapchain);
		blitPass->m_onRun.AddCallback([this, mainColour, swapchain](RenderPassContext& ctx) {
			VkExtent2D extents((uint32_t)ctx.m_renderExtents.x, (uint32_t)ctx.m_renderExtents.y);
			VulkanHelpers::BlitColourImageToImage(ctx.m_graphicsCmds,
				ctx.GetResolvedTarget(mainColour)->m_image, extents,
				ctx.GetResolvedTarget(swapchain)->m_image, extents);
		});
		return blitPass;
	}

	std::unique_ptr<DrawPass> FrameScheduler::MakeImguiPass(const RenderTargetInfo& colourTarget)
	{
		R3_PROF_EVENT();
		auto render = GetSystem<RenderSystem>();
		auto imgui = GetSystem<ImGuiSystem>();
		auto imguiPass = std::make_unique<DrawPass>();			// imgui draw direct to swap image
		imguiPass->m_colourAttachments.push_back({ colourTarget, DrawPass::AttachmentLoadOp::Load });
		imguiPass->m_getExtentsFn = [render]() -> glm::vec2 {
			return render->GetWindowExtents();
		};
		imguiPass->m_onDraw.AddCallback([imgui](RenderPassContext& ctx) {
			imgui->OnDraw(ctx);
		});
		return imguiPass;
	}

	// This describes the entire renderer as a series of passes that run in series
	bool FrameScheduler::BuildRenderGraph()
	{
		R3_PROF_EVENT();		
		auto render = GetSystem<RenderSystem>();

		RenderTargetInfo swapchainImage("Swapchain");	// Swap chain image, this will be presented to the screen each frame
		RenderTargetInfo mainColour("MainColour");		// HDR colour buffer
		mainColour.m_format = GetMainColourTargetFormat();
		mainColour.m_usageFlags = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		RenderTargetInfo mainDepth("MainDepth");		// Main depth buffer
		mainDepth.m_format = GetMainDepthStencilFormat();
		mainDepth.m_usageFlags = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
		mainDepth.m_aspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT;

		auto& graph = render->GetRenderGraph();
		graph.m_allPasses.clear();
		graph.m_allPasses.push_back(MakeMainPass(mainColour, mainDepth));
		graph.m_allPasses.push_back(MakeMainBlitToSwapPass(mainColour, swapchainImage));
		graph.m_allPasses.push_back(MakeImguiPass(swapchainImage));		
		
		return true;
	}
}