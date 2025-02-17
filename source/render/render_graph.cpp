#include "render_graph.h"
#include "device.h"
#include "timestamp_queries_handler.h"
#include "core/profiler.h"
#include "core/log.h"
#include <vulkan/vk_enum_string_helper.h>

namespace R3
{
	

	std::optional<VkImageMemoryBarrier2> DoTransition(RenderTarget* resource, VkPipelineStageFlags2 stageFlags, VkAccessFlags2 access, VkImageLayout layout)
	{
		if (resource->m_lastAccessMode != access || resource->m_lastLayout != layout || resource->m_lastStageFlags != stageFlags)
		{
			VkImageMemoryBarrier2 barrier = { 0 };
			barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
			barrier.srcStageMask = resource->m_lastStageFlags;
			barrier.srcAccessMask = resource->m_lastAccessMode;
			barrier.dstStageMask = stageFlags;
			barrier.dstAccessMask = access;
			barrier.oldLayout = resource->m_lastLayout;
			barrier.newLayout = layout;
			barrier.image = resource->m_image;
			barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

			VkImageSubresourceRange range = { 0 };
			range.aspectMask = resource->m_info.m_aspectFlags;
			range.layerCount = 1;
			range.baseArrayLayer = 0;
			range.levelCount = 1;
			range.baseMipLevel = 0;
			barrier.subresourceRange = range;

			resource->m_lastAccessMode = access;
			resource->m_lastLayout = layout;
			resource->m_lastStageFlags = stageFlags;
			return barrier;
		}
		return {};
	}

	void ComputeDrawPass::ResolveTargets(RenderPassContext& ctx)
	{
		R3_PROF_EVENT();
		std::vector<VkImageMemoryBarrier2> barriers;
		for (const auto& it : m_inputColourAttachments)
		{
			if (auto inputTarget = ctx.m_targets->GetTarget(it))
			{
				ctx.m_resolvedTargets.push_back(inputTarget);
				// input targets must use GENERAL image layout to allow direct read
				auto barrier = DoTransition(inputTarget, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT, VK_IMAGE_LAYOUT_GENERAL);
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
				auto barrier = DoTransition(outputTarget, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL);
				if (barrier)
				{
					barriers.push_back(*barrier);
				}
			}
		}
		
		if (barriers.size() > 0)
		{
			VkDependencyInfo depencyInfo = { 0 };
			depencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
			depencyInfo.imageMemoryBarrierCount = (uint32_t)barriers.size();
			depencyInfo.pImageMemoryBarriers = barriers.data();
			vkCmdPipelineBarrier2(ctx.m_graphicsCmds, &depencyInfo);
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
		std::vector<VkImageMemoryBarrier2> barriers;	// collect the targets needed + any barriers that need to happen
		for (const auto& it : m_inputColourAttachments)
		{
			auto target = ctx.m_targets->GetTarget(it);
			if (target)
			{
				ctx.m_resolvedTargets.push_back(target);
				VkImageLayout newLayout = (target->m_info.m_aspectFlags & VK_IMAGE_ASPECT_DEPTH_BIT) ? VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
				auto barrier = DoTransition(target, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT, VK_ACCESS_2_MEMORY_READ_BIT, newLayout);
				if (barrier)
				{
					barriers.push_back(*barrier);
				}
			}
		}
		for (const auto& it : m_colourAttachments)
		{
			auto target = ctx.m_targets->GetTarget(it.m_info);
			if (target)
			{
				ctx.m_resolvedTargets.push_back(target);
				auto barrier = DoTransition(target, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
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
				auto barrier = DoTransition(target, VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT, VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
				if (barrier)
				{
					barriers.push_back(*barrier);
				}
			}
		}
		if (barriers.size() > 0)
		{
			VkDependencyInfo depencyInfo = { 0 };
			depencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
			depencyInfo.imageMemoryBarrierCount = (uint32_t)barriers.size();
			depencyInfo.pImageMemoryBarriers = barriers.data();
			vkCmdPipelineBarrier2(ctx.m_graphicsCmds, &depencyInfo);
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
		std::vector<VkImageMemoryBarrier2> barriers;	// collect the targets needed + any barriers that need to happen
		for (const auto& it : m_inputs)
		{
			auto target = ctx.m_targets->GetTarget(it);
			if (target)
			{
				ctx.m_resolvedTargets.push_back(target);
				auto barrier = DoTransition(target, VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_READ_BIT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
				if (barrier)
				{
					barriers.push_back(*barrier);
				}
			}
		}
		for (const auto& it : m_outputs)
		{
			auto target = ctx.m_targets->GetTarget(it);
			if (target)
			{
				ctx.m_resolvedTargets.push_back(target);
				auto barrier = DoTransition(target, VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
				if (barrier)
				{
					barriers.push_back(*barrier);
				}
			}
		}
		if (barriers.size() > 0)
		{
			VkDependencyInfo depencyInfo = { 0 };
			depencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
			depencyInfo.imageMemoryBarrierCount = (uint32_t)barriers.size();
			depencyInfo.pImageMemoryBarriers = barriers.data();
			vkCmdPipelineBarrier2(ctx.m_graphicsCmds, &depencyInfo);
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

	void GenericPass::Run(RenderPassContext& ctx)
	{
		R3_PROF_EVENT();
		VulkanHelpers::CommandBufferRegionLabel passLabel(ctx.m_graphicsCmds, m_name, { 1.0f, 1.0f, 1.0f, 1.0f });
		m_onRun.Run(ctx);
	}

	void RenderGraph::Run(GraphContext& context)
	{
		R3_PROF_EVENT();
		RenderPassContext rpc;
		rpc.m_device = context.m_device;
		rpc.m_targets = context.m_targets;
		rpc.m_graphicsCmds = context.m_graphicsCmds;
		for (uint32_t p = 0; p < m_allPasses.size(); ++p)
		{
			auto passTimestamp = context.m_timestampHandler->MakeScopedQuery(m_allPasses[p]->m_name);
			rpc.m_pass = m_allPasses[p].get();
			m_allPasses[p]->Run(rpc);
		}
	}
}