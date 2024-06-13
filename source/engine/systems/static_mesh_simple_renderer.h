#pragma once
#include "core/glm_headers.h"
#include "engine/systems.h"
#include "render/vulkan_helpers.h"
#include "render/writeonly_gpu_buffer.h"
#include "render/descriptors.h"
#include "render/command_buffer_allocator.h"

namespace R3
{
	class Device;
	class DescriptorSetSimpleAllocator;
	class StaticMeshSimpleRenderer : public System
	{
	public:
		StaticMeshSimpleRenderer();
		virtual ~StaticMeshSimpleRenderer();
		static std::string_view GetName() { return "StaticMeshSimpleRenderer"; }
		virtual void RegisterTickFns();
		virtual bool Init();
	private:
		struct GlobalConstants;
		bool ShowGui();
		void Cleanup(Device&);
		bool BuildCommandBuffer();
		void MainPassBegin(Device&, VkCommandBuffer);
		void MainPassDraw(Device&, VkCommandBuffer, const VkExtent2D&);
		bool CreatePipelineData(Device&);
		bool CreateGlobalDescriptorSet();
		void ProcessEnvironmentSettings(GlobalConstants&);
		struct StaticMeshInstanceGpu {
			glm::mat4 m_transform;
			uint32_t m_materialIndex;
		};
		std::unique_ptr<DescriptorSetSimpleAllocator> m_descriptorAllocator;
		VkDescriptorSetLayout_T* m_globalsDescriptorLayout = nullptr;
		VkDescriptorSet_T* m_globalDescriptorSet = nullptr;		// sent to binding 0
		WriteOnlyGpuArray<GlobalConstants> m_globalConstantsBuffer;
		const int c_maxGlobalConstantBuffers = 3;	// ring buffer writes to avoid synchronisation
		int m_currentGlobalConstantsBuffer = 0;
		WriteOnlyGpuArray<StaticMeshInstanceGpu> m_globalInstancesBuffer;	// contains arrays
		const uint32_t c_maxInstances = 1024 * 128;
		const uint32_t c_maxInstanceBuffers = 3;
		uint32_t m_currentInstanceBufferStart = 0;	// index into m_globalInstancesBuffer
		VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
		VkPipeline m_simpleTriPipeline = VK_NULL_HANDLE;
		ManagedCommandBuffer m_thisFrameCmdBuffer;
		bool m_showGui = false;
	};
}