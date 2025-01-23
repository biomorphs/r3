#pragma once
#include "core/glm_headers.h"
#include "engine/systems.h"
#include "render/vulkan_helpers.h"
#include "render/writeonly_gpu_buffer.h"
#include "render/linear_write_gpu_buffer.h"
#include "render/descriptors.h"
#include <unordered_map>

#define USE_LINEAR_BUFFER

namespace R3
{
	// an instance added to a bucket to be drawn (used to generate draw calls)
	struct BucketPartInstance
	{
		uint32_t m_partInstanceIndex;		// index into instance data (gpu memory)
		uint32_t m_partGlobalIndex;			// index into mesh parts array
	};

	// A bucket of draw calls
	struct MeshPartDrawBucket
	{
		std::vector<BucketPartInstance> m_partInstances;	// collected from entities
		uint32_t m_firstDrawOffset;							// index into m_globalInstancesMappedPtr
		uint32_t m_drawCount;								// num. draw indirects
	};

	class Device;
	class DescriptorSetSimpleAllocator;
	class StaticMeshInstanceCullingCompute;
	class Frustum;
	class StaticMeshRenderer : public System
	{
	public:
		StaticMeshRenderer();
		virtual ~StaticMeshRenderer();
		static std::string_view GetName() { return "StaticMeshRenderer"; }
		virtual void RegisterTickFns();
		virtual bool Init();
		void CullInstancesOnGpu(class RenderPassContext& ctx);
		void PrepareForRendering(class RenderPassContext& ctx);		// call from frame graph before CullInstancesOnGpu or drawing anything
		void OnForwardPassDraw(class RenderPassContext& ctx);
		void OnGBufferPassDraw(class RenderPassContext& ctx);
		void OnDrawEnd(class RenderPassContext& ctx);
	private:
		struct GlobalConstants;
		Frustum GetMainCameraFrustum();
		void RebuildStaticScene();									// collect static entities, rebuilds static draw buckets
		void PrepareDrawBucket(MeshPartDrawBucket& bucket);			// write draw indirects with no culling, only used when culling disabled
		void PrepareAndCullDrawBucketCompute(Device&, VkCommandBuffer cmds, VkDeviceAddress instanceDataBuffer, MeshPartDrawBucket& bucket);	// cull instances + write draw indirects
		bool ShowGui();
		bool CollectInstances();									// collects dynamic instances + rebuilds static scene if required
		void Cleanup(Device&);
		bool CreatePipelineLayout(Device&);
		bool CreateForwardPipelineData(Device&, VkFormat mainColourFormat, VkFormat mainDepthFormat);
		bool CreateGBufferPipelineData(Device&, VkFormat positionMetalFormat, VkFormat normalRoughnessFormat, VkFormat albedoAOFormat, VkFormat mainDepthFormat);

		struct StaticMeshInstanceGpu {				// needs to match PerInstanceData in shaders
			glm::mat4 m_transform;
			uint32_t m_materialIndex;
		};
		struct FrameStats {
			uint32_t m_totalModelInstances = 0;
			uint32_t m_totalPartInstances = 0;
			uint32_t m_totalOpaqueInstances = 0;
			uint32_t m_totalTransparentInstances = 0;
			double m_writeCmdsStartTime = 0.0;
			double m_writeCmdsEndTime = 0.0;
			double m_collectInstancesStartTime = 0.0;
			double m_collectInstancesEndTime = 0.0;
			double m_prepareBucketsStartTime = 0.0;
			double m_prepareBucketsEndTime = 0.0;
		};

		FrameStats m_frameStats;

		bool m_enableComputeCulling = true;		// run instance culling in compute
		bool m_lockMainFrustum = false;
		bool m_showGui = false;
		bool m_rebuildStaticScene = true;

		std::unique_ptr<StaticMeshInstanceCullingCompute> m_computeCulling;

		std::unique_ptr<BufferPool> m_meshRenderBufferPool;		// pool used to allocate all buffers

#ifdef USE_LINEAR_BUFFER
		LinearWriteOnlyGpuArray<StaticMeshInstanceGpu> m_staticMeshInstances;	// all *static* instance data written here on static scene rebuild
#else
		WriteOnlyGpuArray<StaticMeshInstanceGpu> m_staticMeshInstances;
#endif
		MeshPartDrawBucket m_staticOpaques;								// all static opaque instances collected here

		const uint32_t c_maxBuffers = 3;		// we reserve space per-frame in globals, draws + dynamic instance data. this determines how many frames to handle
		uint32_t m_thisFrameBuffer = 0;			// determines where to write to globals, draw + dynamic instance data each frame

		WriteOnlyGpuArray<GlobalConstants> m_globalConstantsBuffer;	// globals written here every frame, split into c_maxBuffers sub-buffers
		int m_currentGlobalConstantsBuffer = 0;

		AllocatedBuffer m_drawIndirectHostVisible;	// draw indirect entries for each instance, split into c_maxBuffers sub-buffers. populated every frame from buckets
		void* m_drawIndirectMappedPtr = nullptr;
		VkDeviceAddress m_drawIndirectBufferAddress;
		uint32_t m_currentDrawBufferOffset = 0;		// next write offset for draw data, resets each frame

		const uint32_t c_maxInstances = 1024 * 256;

		VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
		VkPipeline m_forwardPipeline = VK_NULL_HANDLE;
		VkPipeline m_gBufferPipeline = VK_NULL_HANDLE;
	};
}