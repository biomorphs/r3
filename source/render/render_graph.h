#pragma once
#include "engine/callback_array.h"
#include "core/glm_headers.h"
#include <vector>
#include <vulkan/vulkan_core.h>
#include <string>

namespace R3
{
	class Device; 

	// Render graph describes the entire frame + resources that are used
	// The graph is responsible for resource creation + caching, image layout transitions, and any other synchronisation between passes
	// Eventually this will need to handle buffers and other resources
	
	// This describes a render target, everything needed to create/find a cached one
	struct AttachmentInfo
	{
		enum class SizeType {		// allows us to create fixed size (in pixels) or size relative to current swap chain
			Fixed,
			SwapchainMultiple
		};
		std::string m_name;
		SizeType m_sizeType = SizeType::SwapchainMultiple;
		glm::vec2 m_size = { 1,1 };			// either pixels or a multiple of swap chain
		VkFormat m_format = VK_FORMAT_UNDEFINED;
		VkImageUsageFlags m_usageFlags = VK_IMAGE_USAGE_FLAG_BITS_MAX_ENUM;	// must be set for creation
		uint32_t m_samples = 1;
		uint32_t m_levels = 1;
		uint32_t m_layers = 1;
	};

	class RenderPass
	{
	public:
		using BeginCallback = std::function<void(Device&)>;		// called before vkCmdBeginRendering, useful for setup 
		using DrawCallback = std::function<void(Device&)>;		// draw stuff here, called after all input attachments have been properly transitioned
		using EndCallback = std::function<void(Device&)>;		// called after vkCmdEndRendering
		CallbackArray<BeginCallback> m_onPassBegin;
		CallbackArray<DrawCallback> m_onPassDraw;
		CallbackArray<EndCallback> m_onPassEnd;

		std::string m_name;
		std::vector<AttachmentInfo> m_colourInputAttachments;
		AttachmentInfo m_depthStencilAttachment;				// both input and output
		std::vector<AttachmentInfo> m_colourOutputAttachments;

		bool Begin(Device&);
		bool Draw(Device&);
		bool End(Device&);
	};

	class RenderGraph
	{
	public:
		void Run(Device& d);

		std::vector<RenderPass> m_allPasses;					// these are ran in order, nothing fancy
	};
}