#pragma once
#include <memory>
#include "core/glm_headers.h"
#include "render/descriptors.h"

namespace R3
{
	class Device;
	struct RenderTarget;

	// compute shader that runs tonemapping on an input image + outputs to a render target
	// assumes all targets/images are already in correct image layouts
	class TonemapCompute
	{
	public:
		void Run(Device& d, VkCommandBuffer cmds, RenderTarget& hdrTarget, glm::vec2 hdrDimensions, RenderTarget& outputTarget, glm::vec2 outputDimensions);
		bool Initialise(Device& d);
		void Cleanup(Device& d);
		void ShowGui();

	private:

		// track if we need to initialise internal state
		bool m_resourcesInitialised = false;

		enum TonemapType {
			ReinhardColour,
			ReinhardLuminance,
			AGX,
			AGXGolden,
			AGXPunchy,
			Uncharted,
			ACESApprox
		};
		TonemapType m_type = ReinhardColour;

		// descriptor set for tonemap shader
		std::unique_ptr<DescriptorSetSimpleAllocator> m_descriptorAllocator;
		VkDescriptorSetLayout_T* m_descriptorLayout = nullptr;
		static const uint32_t c_maxSets = 3;
		VkDescriptorSet_T* m_descriptorSets[c_maxSets] = { nullptr };
		uint32_t m_currentSet = 0;

		// pipelines for the pass
		VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
		VkPipeline m_pipelineReinhardColour = VK_NULL_HANDLE;
		VkPipeline m_pipelineReinhardLum = VK_NULL_HANDLE;
		VkPipeline m_pipelineAGX = VK_NULL_HANDLE;
		VkPipeline m_pipelineAGXGolden = VK_NULL_HANDLE;
		VkPipeline m_pipelineAGXPunchy = VK_NULL_HANDLE;
		VkPipeline m_pipelineUncharted = VK_NULL_HANDLE;
		VkPipeline m_pipelineACESApprox = VK_NULL_HANDLE;
	};
}