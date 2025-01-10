#pragma once
#include "core/glm_headers.h"
#include "engine/systems.h"
#include "render/vulkan_helpers.h"
#include "render/writeonly_gpu_buffer.h"
#include "render/descriptors.h"
#include <unordered_map>

namespace R3
{
	// Instance of an entire model
	struct StaticMeshInstance
	{
		glm::mat4 m_transform;
		uint32_t m_materialBaseIndex = -1;		// -1 = use model materials
	};

	// An instance of a single model part, with resolved material index
	struct StaticMeshPartInstance
	{
		glm::mat4 m_fullTransform;				// instance * part * model transform (i.e. ready to use)
		uint32_t m_resolvedMaterialIndex;		// actual material index (should always be valid)
		uint32_t m_partGlobalIndex;				// indexes into m_globalInstancesMappedPtr
	};

	using AllMeshInstances = std::unordered_map<uint32_t, std::vector<StaticMeshInstance>>;		// map of model handle -> model instance	
	using AllPartInstances = std::vector<StaticMeshPartInstance>;	// giant array of all part instances populated from AllMeshInstances
	using InstancesBucket = std::vector<uint32_t>;	// indices into AllPartInstances

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

		/////////////////////////////////////////////////////////////////
		// new stuff

		AllMeshInstances m_allMeshInstances;		// all model instances collected from entities (key = model handle id)
		AllPartInstances m_allPartInstances;		// giant array of all part instances expanded from AllMeshInstances

		InstancesBucket m_opaqueInstances;			// indexes into m_allPartInstances
		uint32_t m_opaqueDrawsStart = 0;			// index into m_drawIndirectBuffer
		uint32_t m_opaqueDrawsCount = 0;

		void CollectModelInstancesFromEntities();	// populates m_allMeshInstances
		void CollectModelPartInstances();			// populates m_allPartInstances and m_allDrawBuckets
		void PrepareOpaqueDrawData();				// populates gpu data, generates m_opaqueInstancesStart and m_opaqueInstancesCount for drawing

	private:
		struct GlobalConstants;
		bool ShowGui();
		bool CollectInstances();
		void Cleanup(Device&);
		uint32_t WriteInstances();		// returns instance count
		void MainPassBegin(Device&, VkCommandBuffer, VkFormat mainColourFormat, VkFormat mainDepthFormat);
		void MainPassDraw(Device&, VkCommandBuffer, glm::vec2 extents);
		bool CreatePipelineData(Device&, VkFormat mainColourFormat, VkFormat mainDepthFormat);
		bool CreateGlobalDescriptorSet();
		void ProcessEnvironmentSettings(GlobalConstants&);
		bool InitialiseGpuData(Device&, VkFormat mainColourFormat, VkFormat mainDepthFormat);
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
		uint32_t m_currentInstanceBufferStart = 0;	// index into m_globalInstancesMappedPtr for this frame
		uint32_t m_currentFrameTotalInstances = 0;	// total instances written to global instances this frame

		AllocatedBuffer m_drawIndirectHostVisible;
		void* m_drawIndirectMappedPtr = nullptr;
		uint32_t m_currentDrawBufferStart = 0;		// index into m_drawIndirectHostVisible for this frame
		uint32_t m_currentFrameTotalDraws = 0;		// total draws prepared this frame
		
		const uint32_t c_maxInstances = 1024 * 256;
		const uint32_t c_maxInstanceBuffers = 4;

		VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
		VkPipeline m_simpleTriPipeline = VK_NULL_HANDLE;
		bool m_showGui = false;
	};
}