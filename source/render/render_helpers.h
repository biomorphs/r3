#pragma once
#include <vulkan/vulkan.h>

namespace R3
{
	// Higher-level helpers for interacting with vulkan
	// Has access to the render system 
	namespace RenderHelpers
	{
		// Begin writing to a secondary command buffer that will draw to the main colour + depth
		// Assumed to execute during a render pass
		bool BeginSecondaryCommandBuffer(VkCommandBuffer buffer);

		// Bind a pipeline that will be drawing to the main viewport
		void BindPipeline(VkCommandBuffer buffer, VkPipeline p);
	}
}