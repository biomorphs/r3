#pragma once
#include "engine/systems.h"

enum VkFormat;
namespace R3
{
	class DrawPass;
	class TransferPass;
	struct RenderTargetInfo;
	// Builds + maintains the render graph
	class FrameScheduler : public System
	{
	public:
		static std::string_view GetName() { return "FrameScheduler"; }
		virtual void RegisterTickFns();

		VkFormat GetMainColourTargetFormat();
		VkFormat GetMainDepthStencilFormat();
	private:
		bool BuildRenderGraph();
		std::unique_ptr<DrawPass> MakeMainPass(const RenderTargetInfo& mainColour, const RenderTargetInfo& mainDepth);
		std::unique_ptr<TransferPass> MakeMainBlitToSwapPass(const RenderTargetInfo& mainColour, const RenderTargetInfo& swapchain);
		std::unique_ptr<DrawPass> MakeImguiPass(const RenderTargetInfo& colourTarget);
	};
}