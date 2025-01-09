#include "render_helpers.h"
#include "render_system.h"
#include "vulkan_helpers.h"
#include "core/profiler.h"
#include "core/log.h"

namespace R3
{
	namespace RenderHelpers
	{
		bool BeginSecondaryCommandBuffer(VkCommandBuffer buffer, VkFormat colourFormat, VkFormat depthFormat)
		{
			R3_PROF_EVENT();
			VkCommandBufferBeginInfo beginInfo = { 0 };
			beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			beginInfo.flags = VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;	// this pass is ran inside another render pass
			VkCommandBufferInheritanceInfo bufferInheritance = { 0 };			// we need to pass the attachments info since we are drawing (annoying)
			bufferInheritance.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
			VkCommandBufferInheritanceRenderingInfoKHR dynamicRenderInheritance = { 0 };	// dynamic rendering
			dynamicRenderInheritance.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_RENDERING_INFO_KHR;
			auto colourBufferFormat = colourFormat;
			dynamicRenderInheritance.colorAttachmentCount = 1;
			dynamicRenderInheritance.pColorAttachmentFormats = &colourBufferFormat;
			dynamicRenderInheritance.depthAttachmentFormat = depthFormat;
			dynamicRenderInheritance.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
			bufferInheritance.pNext = &dynamicRenderInheritance;
			beginInfo.pInheritanceInfo = &bufferInheritance;
			if (!VulkanHelpers::CheckResult(vkBeginCommandBuffer(buffer, &beginInfo)))
			{
				LogError("failed to begin recording command buffer!");
				return false;
			}
			return true;
		}

		void BindPipeline(VkCommandBuffer buffer, VkPipeline p)
		{
			R3_PROF_EVENT();
			auto render = Systems::GetSystem<RenderSystem>();
			VkViewport viewport = { 0 };
			viewport.x = 0.0f;
			viewport.y = 0.0f;
			viewport.width = render->GetWindowExtents().x;
			viewport.height = render->GetWindowExtents().y;
			viewport.minDepth = 0.0f;	// normalised! must be between 0 and 1
			viewport.maxDepth = 1.0f;	// ^^
			VkRect2D scissor = { 0 };
			scissor.offset = { 0, 0 };
			scissor.extent = { (uint32_t)viewport.width, (uint32_t)viewport.height };	// draw the full image
			vkCmdBindPipeline(buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, p);
			vkCmdSetViewport(buffer, 0, 1, &viewport);
			vkCmdSetScissor(buffer, 0, 1, &scissor);
		}
		
	}
}