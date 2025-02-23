#pragma once
#include "core/glm_headers.h"
#include "vulkan_helpers.h"
#include <string>
#include <vector>
#include <functional>

namespace R3
{
	// Describes everything we need to create/identify a render target
	struct RenderTargetInfo
	{
		enum class SizeType {		// allows us to create fixed size (in pixels) or size relative to current swap chain
			Fixed,
			SwapchainMultiple
		};
		std::string m_name;
		SizeType m_sizeType = SizeType::SwapchainMultiple;
		glm::vec2 m_size = { 1,1 };			// either pixels or a multiple of swap chain
		VkFormat m_format = VK_FORMAT_UNDEFINED;
		uint32_t m_samples = 1;
		uint32_t m_levels = 1;
		uint32_t m_layers = 1;
		VkImageAspectFlags m_aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;
		VkImageUsageFlags m_usageFlags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		bool operator==(const RenderTargetInfo&);
	};

	// Describes an allocated target + its current state
	struct RenderTarget
	{
		RenderTargetInfo m_info;
		VkImage m_image = VK_NULL_HANDLE;
		VmaAllocation m_allocation = nullptr;	// if set, the cache owns this target
		VkImageView m_view = VK_NULL_HANDLE;
		VkAccessFlags2 m_lastAccessMode = VK_ACCESS_2_NONE;
		VkPipelineStageFlags2 m_lastStageFlags = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
		VkImageLayout m_lastLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	};

	class RenderTargetCache
	{
	public:
		RenderTargetCache();
		~RenderTargetCache();
		glm::vec2 GetTargetSize(const RenderTargetInfo& info);
		RenderTarget* GetTarget(const RenderTargetInfo& info);	// will create a new one if no matching target found
		void AddTarget(const RenderTargetInfo& info, VkImage image, VkImageView view);	// add an external target to the cache
		void Clear();	// remove all existing targets + free memory
		void ResetForNewFrame();	// reset all previous access mode/layouts for a new frame

		using EnumerateTargetFn = std::function<void(const RenderTargetInfo&, size_t)>;	// target info, allocation size in bytes
		void EnumerateTargets(EnumerateTargetFn);

	private:
		std::vector<RenderTarget> m_allTargets;
	};
}