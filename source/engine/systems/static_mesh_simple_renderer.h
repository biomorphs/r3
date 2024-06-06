#pragma once
#include "engine/systems.h"
#include "render/vulkan_helpers.h"
#include "render/writeonly_gpu_buffer.h"
#include "render/descriptors.h"

namespace R3
{
	class Device;
	class StaticMeshSimpleRenderer : public System
	{
	public:
		StaticMeshSimpleRenderer();
		virtual ~StaticMeshSimpleRenderer();
		static std::string_view GetName() { return "StaticMeshSimpleRenderer"; }
		virtual void RegisterTickFns();
		virtual bool Init();
	private:
		bool ShowGui();
		void Cleanup(Device&);
		void MainPassBegin(Device&, VkCommandBuffer);
		void MainPassDraw(Device&, VkCommandBuffer, const VkExtent2D&);
		bool CreatePipelineData(Device&);

		struct GlobalConstants;
		std::unique_ptr<DescriptorSetSimpleAllocator> m_descriptorAllocator;
		VkDescriptorSet m_allTexturesSet;	// invalidated/released each frame (copy on write)
		WriteOnlyGpuArray<GlobalConstants> m_globalConstantsBuffer;
		const int c_maxGlobalConstantBuffers = 3;	// ring buffer writes to avoid synchronisation
		int m_currentGlobalConstantsBuffer = 0;
		VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
		VkPipeline m_simpleTriPipeline = VK_NULL_HANDLE;
		bool m_showGui = false;
	};
}