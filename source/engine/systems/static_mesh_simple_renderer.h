#pragma once
#include "core/glm_headers.h"
#include "engine/systems.h"
#include "render/vulkan_helpers.h"
#include "render/writeonly_gpu_buffer.h"
#include "render/descriptors.h"
#include <unordered_map>

namespace R3
{
	// A bucket of draw calls
	struct MeshPartDrawBucket
	{
		struct BucketPartInstance
		{
			uint32_t m_partInstanceIndex;		// index into instance data (gpu memory)
			uint32_t m_partGlobalIndex;			// index into mesh parts aarray
		};
		std::vector<BucketPartInstance> m_partInstances;	// collected from entities
		uint32_t m_firstDrawOffset;						// index into m_globalInstancesMappedPtr
		uint32_t m_drawCount;							// num. draw indirects
	};

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
		void PrepareForRendering(class RenderPassContext& ctx);		// call from frame graph before drawing anything
		void OnForwardPassDraw(class RenderPassContext& ctx);
		void OnGBufferPassDraw(class RenderPassContext& ctx);
		void OnDrawEnd(class RenderPassContext& ctx);
		inline glm::vec4 GetMainColourClearValue() { return m_mainPassColourClearValue; }

	private:
		struct GlobalConstants;
		void CollectAllPartInstances();
		void PrepareDrawBucket(MeshPartDrawBucket& bucket);
		bool ShowGui();
		bool CollectInstances();
		void Cleanup(Device&);
		bool CreatePipelineLayout(Device&);
		bool CreateForwardPipelineData(Device&, VkFormat mainColourFormat, VkFormat mainDepthFormat);
		bool CreateGBufferPipelineData(Device&, VkFormat positionMetalFormat, VkFormat normalRoughnessFormat, VkFormat albedoAOFormat, VkFormat mainDepthFormat);
		bool CreateGlobalDescriptorSet();
		struct StaticMeshInstanceGpu {				// needs to match PerInstanceData in shaders
			glm::mat4 m_transform;
			uint32_t m_materialIndex;
		};
		struct FrameStats {
			uint32_t m_totalModelInstances = 0;
			uint32_t m_totalPartInstances = 0;
			uint32_t m_totalOpaqueInstances = 0;
			uint32_t m_totalTransparentInstances = 0;
			uint32_t m_totalTriangles = 0;
			double m_writeCmdsStartTime = 0.0;
			double m_writeCmdsEndTime = 0.0;
			double m_collectInstancesStartTime = 0.0;
			double m_collectInstancesEndTime = 0.0;
			double m_prepareBucketsStartTime = 0.0;
			double m_prepareBucketsEndTime = 0.0;
		};

		MeshPartDrawBucket m_allOpaques;
		MeshPartDrawBucket m_allTransparents;
		bool m_forwardRenderEverything = false;	// override to pass all instances to forward pass

		glm::vec4 m_mainPassColourClearValue = { 0,0,0,1 };
		FrameStats m_frameStats;

		std::unique_ptr<DescriptorSetSimpleAllocator> m_descriptorAllocator;
		VkDescriptorSetLayout_T* m_globalsDescriptorLayout = nullptr;
		VkDescriptorSet_T* m_globalDescriptorSet = nullptr;	

		WriteOnlyGpuArray<GlobalConstants> m_globalConstantsBuffer;
		const int c_maxGlobalConstantBuffers = 3;	// ring buffer writes to avoid synchronisation
		int m_currentGlobalConstantsBuffer = 0;

		AllocatedBuffer m_globalInstancesHostVisible;
		StaticMeshInstanceGpu* m_globalInstancesMappedPtr = nullptr;
		uint32_t m_currentInstanceBufferStart = 0;	// index into m_globalInstancesMappedPtr for this frame
		uint32_t m_currentInstanceBufferOffset = 0;	// offset from m_currentInstanceBufferStart

		AllocatedBuffer m_drawIndirectHostVisible;
		void* m_drawIndirectMappedPtr = nullptr;
		uint32_t m_currentDrawBufferStart = 0;		// base index into m_drawIndirectHostVisible for this frame
		uint32_t m_currentDrawBufferOffset = 0;		// offset from m_currentDrawBufferStart

		const uint32_t c_maxInstances = 1024 * 256;
		const uint32_t c_maxInstanceBuffers = 3;

		VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
		VkPipeline m_forwardPipeline = VK_NULL_HANDLE;
		VkPipeline m_gBufferPipeline = VK_NULL_HANDLE;
		bool m_showGui = false;
	};
}