#pragma once
#include "core/glm_headers.h"
#include <vector>
#include <string_view>

struct VkCommandBuffer_T;
namespace R3
{
	struct RenderTarget;	
	struct RenderTargetInfo;
	class RenderTargetCache;
	class RenderGraphPass;
	class Device;

	// Passed to each render pass
	class RenderPassContext
	{
	public:
		Device* m_device = nullptr;				// device used for rendering
		RenderGraphPass* m_pass = nullptr;		// the currently running pass
		RenderTargetCache* m_targets = nullptr;	// render target cache
		VkCommandBuffer_T* m_graphicsCmds = nullptr;	// main cmd buffer, supports graphics + compute
		std::vector<RenderTarget*> m_resolvedTargets;	// all targets used by a pass
		glm::vec2 m_renderExtents;		// extents of render pass
		uint32_t m_previousStageFlags = 0;	// used when submitting pipeline barriers during resolve

		RenderTarget* GetResolvedTarget(const RenderTargetInfo& info);
		RenderTarget* GetResolvedTarget(std::string_view targetName);
	};
}