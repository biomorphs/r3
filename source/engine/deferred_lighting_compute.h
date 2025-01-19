#pragma once
#include "core/glm_headers.h"
#include "render/descriptors.h"
#include <memory>

namespace R3
{
	class Device;
	struct RenderTarget;

	class DeferredLightingCompute
	{
	public:
		void Run(Device& d, VkCommandBuffer cmds, 
			RenderTarget& depthBuffer,
			RenderTarget& positionMetalTarget,
			RenderTarget& normalRoughnessTarget,
			RenderTarget& albedoAOTarget,
			RenderTarget& outputTarget, glm::vec2 outputDimensions);
		bool Initialise(Device& d);
		void Cleanup(Device& d);

	private:
		// track if we need to initialise internal state
		bool m_resourcesInitialised = false;

		// per-frame descriptor sets + allocator
		std::unique_ptr<DescriptorSetSimpleAllocator> m_descriptorAllocator;
		VkDescriptorSetLayout_T* m_descriptorLayout = nullptr;
		static const uint32_t c_maxSets = 3;
		VkDescriptorSet_T* m_descriptorSets[c_maxSets] = { nullptr };
		uint32_t m_currentSet = 0;

		// Can't use depth buffer as storage image, sample it as a texture instead
		VkSampler m_depthSampler = VK_NULL_HANDLE;

		// pipeline for the pass
		VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
		VkPipeline m_pipeline = VK_NULL_HANDLE;
	};
}