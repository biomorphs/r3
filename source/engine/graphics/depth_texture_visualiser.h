#pragma once
#include "core/glm_headers.h"
#include "render/descriptors.h"
#include <memory>

namespace R3
{
	class Device;
	struct RenderTarget;

	// Used to render a depth texture to another render target
	// Has controls for adjusting min/max value thresholds and scale/offset
	class DepthTextureVisualiser
	{
	public:
		void Run(Device& d, VkCommandBuffer cmds, RenderTarget& depthBuffer, glm::vec2 depthDimensions, RenderTarget& outputTarget, glm::vec2 outputDimensions);
		void Cleanup(Device& d);
		void ShowGui();

	private:
		bool Initialise(Device& d);

		// track if we need to initialise internal state
		bool m_resourcesInitialised = false;
		float m_minValue = 0.0f;	// min/max values of depth for scaling
		float m_maxValue = 1.0f;
		float m_scale = 1.0f;				// scale/offset
		glm::vec2 m_offset = { 0,0 };

		// per-frame descriptor sets + allocator
		std::unique_ptr<DescriptorSetSimpleAllocator> m_descriptorAllocator;
		VkDescriptorSetLayout_T* m_descriptorLayout = nullptr;
		static const uint32_t c_maxSets = 3;
		VkDescriptorSet_T* m_descriptorSets[c_maxSets] = { nullptr };
		uint32_t m_currentSet = 0;

		// Can't use depth buffer as storage image, sample it as a texture instead
		VkSampler m_depthSampler = VK_NULL_HANDLE;

		// pipelines + layouts
		VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
		VkPipeline m_pipeline = VK_NULL_HANDLE;
	};
}