#pragma once
#include "core/callback_array.h"
#include "core/glm_headers.h"
#include "vulkan_helpers.h"
#include "render_target_cache.h"
#include "render_pass_context.h"
#include <vector>
#include <string>
#include <memory>
#include <optional>

namespace R3
{
	class Device;
	class TimestampQueriesHandler;

	// base render graph pass node
	class RenderGraphPass
	{
	public:
		virtual ~RenderGraphPass() = default;
		virtual void Run(RenderPassContext&) = 0;
		std::string m_name;
	};

	// a generic render pass, useful for uploading + prepping data before drawing, etc
	class GenericPass : public RenderGraphPass
	{
	public:
		virtual ~GenericPass() = default;
		virtual void Run(RenderPassContext& ctx);
		using RunCallback = std::function<void(RenderPassContext&)>;
		CallbackArray<RunCallback> m_onRun;	// do the work here
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

	// runs compute cmds that will write to a specific colour render target
	class ComputeDrawPass : public RenderGraphPass
	{
	public:
		virtual ~ComputeDrawPass() = default;
		virtual void Run(RenderPassContext& ctx);
		void ResolveTargets(RenderPassContext& ctx);
		using RunCallback = std::function<void(RenderPassContext&)>;
		std::vector<RenderTargetInfo> m_inputColourAttachments;	// targets used as input
		std::vector<RenderTargetInfo> m_outputColourAttachments;	// write output to these attachments
		CallbackArray<RunCallback> m_onRun;		// do the compute work here
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
		std::vector<RenderTargetInfo> m_inputColourAttachments;	// targets used as input
		std::vector<DrawAttachment> m_colourAttachments;	// draw to these
		std::optional<DrawAttachment> m_depthAttachment;	// draw/use these
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
			Device* m_device = nullptr;
			RenderTargetCache* m_targets = nullptr;
			VkCommandBuffer m_graphicsCmds;	// main cmd buffer
			TimestampQueriesHandler* m_timestampHandler = nullptr;
		};
		void Run(GraphContext& context);

		std::vector<std::unique_ptr<RenderGraphPass>> m_allPasses;					// these are ran in order, nothing fancy
	};	
}