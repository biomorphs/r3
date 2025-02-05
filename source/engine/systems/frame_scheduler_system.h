#pragma once
#include "engine/systems.h"

enum VkFormat;
namespace R3
{
	class DrawPass;
	class ComputeDrawPass;
	class TransferPass;
	class GenericPass;
	struct RenderTargetInfo;
	class TonemapCompute;
	class DeferredLightingCompute;
	class TiledLightsCompute;
	class DepthTextureVisualiser;
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
		bool UpdateTonemapper();

		std::unique_ptr<GenericPass> MakeRenderPreparePass();	// sets stuff up before other render passes
		std::unique_ptr<ComputeDrawPass> MakeCullingPass();
		std::unique_ptr<ComputeDrawPass> MakeLightTilingPass(const RenderTargetInfo& mainDepth);
		std::unique_ptr<DrawPass> MakeShadowCascadePass(const RenderTargetInfo& cascadeTarget, int cascadeIndex);
		std::unique_ptr<DrawPass> MakeForwardPass(const RenderTargetInfo& mainColour, const RenderTargetInfo& mainDepth);
		std::unique_ptr<DrawPass> MakeGBufferPass(const RenderTargetInfo& positionBuffer, const RenderTargetInfo& normalBuffer, const RenderTargetInfo& albedoBuffer, const RenderTargetInfo& mainDepth);
		std::unique_ptr<ComputeDrawPass> MakeDeferredLightingPass(const RenderTargetInfo& mainDepth, const RenderTargetInfo& positionBuffer, const RenderTargetInfo& normalBuffer, const RenderTargetInfo& albedoBuffer, const RenderTargetInfo& mainColour);
		std::unique_ptr<ComputeDrawPass> MakeTonemapToLDRPass(const RenderTargetInfo& mainColour, const RenderTargetInfo& mainColourLDR);
		std::unique_ptr<TransferPass> MakeColourBlitToPass(std::string_view name, const RenderTargetInfo& srcTarget, const RenderTargetInfo& destTarget);
		std::unique_ptr<ComputeDrawPass> MakeDepthTextureDebugPass(const RenderTargetInfo& depthTexture, const RenderTargetInfo& outputTexture);
		std::unique_ptr<DrawPass> MakeImguiPass(const RenderTargetInfo& colourTarget);
		std::unique_ptr<ComputeDrawPass> MakeLightTileDebugPass(const RenderTargetInfo& colourTarget);

		std::unique_ptr<TonemapCompute> m_tonemapComputeRenderer;
		std::unique_ptr<DeferredLightingCompute> m_deferredLightingCompute;
		std::unique_ptr<TiledLightsCompute> m_tiledLightsCompute;
		std::unique_ptr<DepthTextureVisualiser> m_depthTextureVisualiser;
		std::vector<RenderTargetInfo> m_allCurrentTargets;	// keep the list of all render targets around for debugging

		// Colour target visualiser (blits a target to swap chain)
		bool m_renderTargetDebuggerEnabled = false;
		std::string m_debugRenderTargetName;
		
		bool m_useTiledLighting = true;
		bool m_buildLightTilesOnCpu = false;
		bool m_showLightTilesDebug = false;
		uint64_t m_lightTileDebugMetadataAddress = 0;	// keep track of the main colour buffer light tiles for debugging
	};
}