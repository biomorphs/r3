#pragma once
#include "engine/callback_array.h"
#include "core/glm_headers.h"
#include "vulkan_helpers.h"
#include "render_target_cache.h"
#include <vector>
#include <string>
#include <memory>
#include <optional>

namespace R3
{
	// Passed to each render pass
	class RenderPassContext
	{
	public:
		RenderTargetCache* m_targets;	// target cache
		VkCommandBuffer m_graphicsCmds;	// main cmd buffer
		std::vector<RenderTarget*> m_resolvedTargets;	// all targets used by a pass
		glm::vec2 m_renderExtents;		// extents of render pass

		RenderTarget* GetResolvedTarget(const RenderTargetInfo& info);
	};

	// base render graph pass node
	class RenderGraphPass
	{
	public:
		virtual ~RenderGraphPass() = default;
		virtual void Run(RenderPassContext&) = 0;
		std::string m_name;
	};

	// used to move data between render targets using transfer operations
	class TransferPass : public RenderGraphPass
	{
	public:
		virtual ~TransferPass() = default;
		virtual void Run(RenderPassContext& ctx);
		void ResolveTargets(RenderPassContext& ctx);
		std::vector<RenderTargetInfo> m_inputs;
		std::vector<RenderTargetInfo> m_outputs;
		using RunCallback = std::function<void(RenderPassContext&)>;
		CallbackArray<RunCallback> m_onRun;	// do the work here
	};

	// used to draw to a set of targets
	class DrawPass : public RenderGraphPass
	{
	public:
		virtual ~DrawPass() = default;
		virtual void Run(RenderPassContext& ctx);
		void ResolveTargets(RenderPassContext& ctx);
		enum AttachmentLoadOp {
			DontCare,
			Load,
			Clear
		};
		struct DrawAttachment {	// need to describe the load operation when drawing to a target
			RenderTargetInfo m_info;
			AttachmentLoadOp m_loadOp = AttachmentLoadOp::Load;
		};
		std::vector<DrawAttachment> m_colourAttachments;
		std::optional<DrawAttachment> m_depthAttachment;
		std::function<glm::vec2()> m_getExtentsFn;
		std::function<glm::vec4()> m_getClearColourFn;
		std::function<float()> m_getClearDepthFn;

		using RunCallback = std::function<void(RenderPassContext&)>;
		CallbackArray<RunCallback> m_onBegin;	// called before vkCmdBeginRendering, useful for setup 
		CallbackArray<RunCallback> m_onDraw;	// draw stuff here
		CallbackArray<RunCallback> m_onEnd;		// called after vkCmdEndRendering
	};

	class RenderGraph
	{
	public:
		struct GraphContext
		{
			RenderTargetCache* m_targets = nullptr;
			VkCommandBuffer m_graphicsCmds;	// main cmd buffer
		};
		void Run(GraphContext& context);

		std::vector<std::unique_ptr<RenderGraphPass>> m_allPasses;					// these are ran in order, nothing fancy
	};	
}