#include "render_graph.h"
#include "device.h"
#include "timestamp_queries_handler.h"
#include "core/profiler.h"
#include "core/log.h"

namespace R3
{
	

	std::optional<VkImageMemoryBarrier> DoTransition(RenderTarget* resource, VkAccessFlagBits access, VkImageLayout layout)
	{
		if (resource->m_lastAccessMode != access || resource->m_lastLayout != layout)
		{
			auto barrier = VulkanHelpers::MakeImageBarrier(resource->m_image, resource->m_info.m_aspectFlags,
				VK_ACCESS_NONE, access, resource->m_lastLayout, layout);	// we dont care about src access mask
			resource->m_lastAccessMode = access;
			resource->m_lastLayout = layout;
			return barrier;
		}
		return {};
	}

	void ComputeDrawPass::ResolveTargets(RenderPassContext& ctx)
	{
		R3_PROF_EVENT();
		std::vector<VkImageMemoryBarrier> barriers;

		for (const auto& it : m_inputColourAttachments)
		{
			if (auto inputTarget = ctx.m_targets->GetTarget(it))
			{
				ctx.m_resolvedTargets.push_back(inputTarget);
				// input targets must use GENERAL image layout to allow direct read
				auto barrier = DoTransition(inputTarget, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_GENERAL);
				if (barrier)
				{
					barriers.push_back(*barrier);
				}
			}
		}
		for (const auto& it : m_outputColourAttachments)
		{
			if (auto outputTarget = ctx.m_targets->GetTarget(it))
			{
				ctx.m_resolvedTargets.push_back(outputTarget);
				// output targets must use GENERAL image layout to allow direct write
				auto barrier = DoTransition(outputTarget, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL);
				if (barrier)
				{
					barriers.push_back(*barrier);
				}
			}
		}
		
		if (barriers.size() > 0)
		{
			auto srcStages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;	// barrier must happen between output to attachment + before compute
			auto dstStages = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
			vkCmdPipelineBarrier(ctx.m_graphicsCmds, srcStages, dstStages, 0, 0, nullptr, 0, nullptr, (uint32_t)barriers.size(), barriers.data());
		}
	}

	void ComputeDrawPass::Run(RenderPassContext& ctx)
	{
		R3_PROF_EVENT();
		ResolveTargets(ctx);
		{
			VulkanHelpers::CommandBufferRegionLabel passLabel(ctx.m_graphicsCmds, m_name, {0.2f, 0.2f, 1.0f, 1.0f});
			m_onRun.Run(ctx);
		}
	}

	void DrawPass::ResolveTargets(RenderPassContext& ctx)
	{
		R3_PROF_EVENT();
		std::vector<VkImageMemoryBarrier> barriers;	// collect the targets needed + any barriers that need to happen
		for (const auto& it : m_colourAttachments)
		{
			auto target = ctx.m_targets->GetTarget(it.m_info);
			if (target)
			{
				ctx.m_resolvedTargets.push_back(target);
				auto barrier = DoTransition(target, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
				if (barrier)
				{
					barriers.push_back(*barrier);
				}
			}
		}
		if (m_depthAttachment)
		{
			auto target = ctx.m_targets->GetTarget(m_depthAttachment->m_info);
			if (target)
			{
				ctx.m_resolvedTargets.push_back(target);
				auto barrier = DoTransition(target, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
				if (barrier)
				{
					barriers.push_back(*barrier);
				}
			}
		}
		if (barriers.size() > 0)
		{
			auto dstStages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
			vkCmdPipelineBarrier(ctx.m_graphicsCmds, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, dstStages, 0, 0, nullptr, 0, nullptr, (uint32_t)barriers.size(), barriers.data());
		}
	}

	VkAttachmentLoadOp GetVkLoadOp(DrawPass::AttachmentLoadOp op)
	{
		switch (op)
		{
		case DrawPass::AttachmentLoadOp::Clear:
			return VK_ATTACHMENT_LOAD_OP_CLEAR;
		case DrawPass::AttachmentLoadOp::Load:
			return VK_ATTACHMENT_LOAD_OP_LOAD;
		case DrawPass::AttachmentLoadOp::DontCare:
		default:
			return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		}
	}

	void DrawPass::Run(RenderPassContext& ctx)
	{
		R3_PROF_EVENT();
		ctx.m_renderExtents = m_getExtentsFn();
		ResolveTargets(ctx);	// populate list of actual targets
		{
			VulkanHelpers::CommandBufferRegionLabel passLabel(ctx.m_graphicsCmds, m_name, { 0.2f, 1.0f, 0.2f, 1.0f });
			m_onBegin.Run(ctx);		// run the initial callbacks

			VkClearColorValue clearColour = { 0, 0, 0, 1 };
			if (m_getClearColourFn)
			{
				glm::vec4 c = m_getClearColourFn();
				clearColour = { c.x, c.y, c.z, c.w };
			}
			float clearDepth = 1.0f;
			if (m_getClearDepthFn)
			{
				clearDepth = m_getClearDepthFn();
			}
			// Now set up the attachments for drawing
			std::vector<VkRenderingAttachmentInfo> colourAttachments;
			VkRenderingAttachmentInfo depthAttachment = {};
			for (int i = 0; i < m_colourAttachments.size(); ++i)
			{
				VkRenderingAttachmentInfo rai = {};
				rai.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
				rai.clearValue.color = clearColour;
				rai.loadOp = GetVkLoadOp(m_colourAttachments[i].m_loadOp);
				rai.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
				rai.imageView = ctx.GetResolvedTarget(m_colourAttachments[i].m_info)->m_view;
				rai.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;	// check
				colourAttachments.push_back(rai);
			}
			VkRenderingInfo ri = {};
			ri.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
			ri.colorAttachmentCount = (uint32_t)colourAttachments.size();
			ri.pColorAttachments = colourAttachments.data();
			ri.layerCount = 1;
			ri.renderArea.offset = { 0,0 };
			ri.renderArea.extent = { (uint32_t)ctx.m_renderExtents.x, (uint32_t)ctx.m_renderExtents.y };
			ri.flags = VK_RENDERING_CONTENTS_SECONDARY_COMMAND_BUFFERS_BIT;		// can execute secondary command buffers
			if (m_depthAttachment)
			{
				depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
				depthAttachment.clearValue.depthStencil = { clearDepth, 0 };
				depthAttachment.loadOp = GetVkLoadOp(m_depthAttachment->m_loadOp);
				depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
				depthAttachment.imageView = ctx.GetResolvedTarget(m_depthAttachment->m_info)->m_view;
				depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;	// check this
				ri.pDepthAttachment = &depthAttachment;
			}
			vkCmdBeginRendering(ctx.m_graphicsCmds, &ri);
			m_onDraw.Run(ctx);
			vkCmdEndRendering(ctx.m_graphicsCmds);
			m_onEnd.Run(ctx);
		}
	}

	void TransferPass::ResolveTargets(RenderPassContext& ctx)
	{
		R3_PROF_EVENT();
		std::vector<VkImageMemoryBarrier> inBarriers, outBarriers;	// collect the targets needed + any barriers that need to happen
		for (const auto& it : m_inputs)
		{
			auto target = ctx.m_targets->GetTarget(it);
			if (target)
			{
				ctx.m_resolvedTargets.push_back(target);
				auto barrier = DoTransition(target, VK_ACCESS_TRANSFER_READ_BIT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
				if (barrier)
				{
					inBarriers.push_back(*barrier);
				}
			}
		}
		for (const auto& it : m_outputs)
		{
			auto target = ctx.m_targets->GetTarget(it);
			if (target)
			{
				ctx.m_resolvedTargets.push_back(target);
				auto barrier = DoTransition(target, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
				if (barrier)
				{
					outBarriers.push_back(*barrier);
				}
			}
		}
		if (inBarriers.size() > 0)	// transition inputs before transfers
		{
			auto dstStages = VK_PIPELINE_STAGE_TRANSFER_BIT;
			vkCmdPipelineBarrier(ctx.m_graphicsCmds, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, dstStages, 0, 0, nullptr, 0, nullptr, (uint32_t)inBarriers.size(), inBarriers.data());
		}
		if (outBarriers.size() > 0)	// transition outputs after transfer + before draw
		{
			auto dstStages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			vkCmdPipelineBarrier(ctx.m_graphicsCmds, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, dstStages, 0, 0, nullptr, 0, nullptr, (uint32_t)outBarriers.size(), outBarriers.data());
		}
	}

	void TransferPass::Run(RenderPassContext& ctx)
	{
		R3_PROF_EVENT();
		ResolveTargets(ctx);
		{
			VulkanHelpers::CommandBufferRegionLabel passLabel(ctx.m_graphicsCmds, m_name, { 1.0f, 0.2f, 0.2f, 1.0f });
			m_onRun.Run(ctx);
		}
	}

	void RenderGraph::Run(GraphContext& context)
	{
		R3_PROF_EVENT();
		for (uint32_t p = 0; p < m_allPasses.size(); ++p)
		{
			auto passTimestamp = context.m_timestampHandler->MakeScopedQuery(m_allPasses[p]->m_name);

			RenderPassContext rpc;
			rpc.m_device = context.m_device;
			rpc.m_pass = m_allPasses[p].get();
			rpc.m_targets = context.m_targets;
			rpc.m_graphicsCmds = context.m_graphicsCmds;
			m_allPasses[p]->Run(rpc);
		}
	}
}