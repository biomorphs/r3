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
		void OnMainPassBegin(class RenderPassContext& ctx);
		void OnMainPassDraw(class RenderPassContext& ctx);
		inline glm::vec4 GetMainColourClearValue() { return m_mainPassColourClearValue; }
	private:
		struct GlobalConstants;
		bool ShowGui();
		void Cleanup(Device&);
		bool BuildCommandBuffer();
		uint32_t WriteInstances(VkCommandBuffer_T* buffer);		// returns instance count
		void MainPassBegin(Device&, VkCommandBuffer);
		void MainPassDraw(Device&, VkCommandBuffer);
		bool CreatePipelineData(Device&);
		bool CreateGlobalDescriptorSet();
		void ProcessEnvironmentSettings(GlobalConstants&);
		bool InitialiseGpuData(Device&);
		struct StaticMeshInstanceGpu {				// needs to match PerInstanceData in shaders
			glm::mat4 m_transform;
			uint32_t m_materialIndex;
		};
		struct FrameStats {
			uint32_t m_totalModelInstances = 0;
			uint32_t m_totalPartInstances = 0;
			uint32_t m_totalTriangles = 0;
			double m_writeCmdsStartTime = 0.0;
			double m_writeCmdsEndTime = 0.0;
		};
		glm::vec4 m_mainPassColourClearValue = { 0,0,0,1 };
		FrameStats m_frameStats;
		std::unique_ptr<DescriptorSetSimpleAllocator> m_descriptorAllocator;
		VkDescriptorSetLayout_T* m_globalsDescriptorLayout = nullptr;
		VkDescriptorSet_T* m_globalDescriptorSet = nullptr;	
		WriteOnlyGpuArray<GlobalConstants> m_globalConstantsBuffer;
		const int c_maxGlobalConstantBuffers = 4;	// ring buffer writes to avoid synchronisation
		int m_currentGlobalConstantsBuffer = 0;
		AllocatedBuffer m_globalInstancesHostVisible;
		StaticMeshInstanceGpu* m_globalInstancesMappedPtr = nullptr;
		AllocatedBuffer m_drawIndirectHostVisible;
		void* m_drawIndirectMappedPtr = nullptr;
		uint32_t m_currentInstanceBufferStart = 0;	// index into m_globalInstancesMappedPtr and m_drawIndirectBuffer
		const uint32_t c_maxInstances = 1024 * 256;
		const uint32_t c_maxInstanceBuffers = 4;
		VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
		VkPipeline m_simpleTriPipeline = VK_NULL_HANDLE;
		ManagedCommandBuffer m_thisFrameCmdBuffer;
		bool m_showGui = false;
	};
}