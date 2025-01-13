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
		bool ShowGui();
		bool BuildRenderGraph();

		std::unique_ptr<DrawPass> MakeMainPass(const RenderTargetInfo& mainColour, const RenderTargetInfo& mainDepth);
		std::unique_ptr<ComputeDrawPass> MakeTonemapToLDRPass(const RenderTargetInfo& mainColour, const RenderTargetInfo& mainColourLDR);
		std::unique_ptr<TransferPass> MakeColourBlitToPass(std::string_view name, const RenderTargetInfo& srcTarget, const RenderTargetInfo& destTarget);
		std::unique_ptr<DrawPass> MakeImguiPass(const RenderTargetInfo& colourTarget);
		std::unique_ptr<TonemapCompute> m_tonemapComputeRenderer;

		std::vector<RenderTargetInfo> m_allCurrentTargets;	// keep the list of all render targets around for debugging

		// Colour target visualiser (blits a target to swap chain)
		bool m_colourTargetDebuggerEnabled = true;
		std::string m_colourDebugTargetName;
	};
}