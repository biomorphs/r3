#pragma once
#include "engine/systems.h"

enum VkFormat;
namespace R3
{
	class DrawPass;
	class ComputeDrawPass;
	class TransferPass;
	struct RenderTargetInfo;
	class TonemapCompute;
	// Builds + maintains the render graph
	class FrameScheduler : public System
	{
	public:
		FrameScheduler();
		virtual ~FrameScheduler();
		static std::string_view GetName() { return "FrameScheduler"; }
		virtual void RegisterTickFns();
		virtual bool Init();
	private:
		bool BuildRenderGraph();
		std::unique_ptr<DrawPass> MakeMainPass(const RenderTargetInfo& mainColour, const RenderTargetInfo& mainDepth);
		std::unique_ptr<ComputeDrawPass> MakeTonemapToLDRPass(const RenderTargetInfo& mainColour, const RenderTargetInfo& mainColourLDR);
		std::unique_ptr<TransferPass> MakeMainBlitToSwapPass(const RenderTargetInfo& mainColour, const RenderTargetInfo& swapchain);
		std::unique_ptr<DrawPass> MakeImguiPass(const RenderTargetInfo& colourTarget);

		std::unique_ptr<TonemapCompute> m_tonemapComputeRenderer;
	};
}