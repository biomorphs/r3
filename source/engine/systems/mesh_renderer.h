#pragma once
#include "core/glm_headers.h"
#include "engine/systems.h"
#include "render/vulkan_helpers.h"
#include "render/writeonly_gpu_buffer.h"
#include "render/linear_write_gpu_buffer.h"
#include "render/descriptors.h"
#include <unordered_map>

namespace R3
{
	// Instance data passed for each model part draw call
	struct MeshInstance							// needs to match PerInstanceData in shaders
	{				
		glm::mat4 m_transform;					// final part world-space transform
		VkDeviceAddress m_materialDataAddress;	// in order to support multiple material buffers, we pass the address to the material directly for every instance
	};

	// an instance added to a bucket to be drawn (used to generate draw calls)
	struct BucketPartInstance
	{
		uint32_t m_partInstanceIndex;		// index into instance data (gpu memory)
		uint32_t m_partGlobalIndex;			// index into mesh parts array
	};

	// A bucket of instances associated with a particular pass/shader/technique
	// i.e. one for gbuffer, forward pass, shadows, etc
	struct MeshPartInstanceBucket
	{
		std::vector<BucketPartInstance> m_partInstances;	// collected from entities
	};

	// Draw data associated with a bucket, this references draw indirects
	// Built from the MeshPartInstanceBucket instances on cpu or compute, there can be multiple per bucket
	struct MeshPartBucketDrawIndirects
	{
		uint32_t m_firstDrawOffset;							// index into m_globalInstancesMappedPtr
		uint32_t m_drawCount;								// num. draw indirects
	};

	class Device;
	class MeshInstanceCullingCompute;
	class Frustum;
	struct RenderTargetInfo;
	struct MeshMaterial;
	struct ModelDataHandle;
	class MeshRenderer : public System
	{
	public:
		MeshRenderer();
		virtual ~MeshRenderer();
		static std::string_view GetName() { return "MeshRenderer"; }
		virtual void RegisterTickFns();
		virtual bool Init();
		void SetStaticsDirty();										// calling this will trigger a full rebuild of all static instances for 1 frame
		void PrepareForRendering(class RenderPassContext& ctx);		// call from frame graph before CullInstancesOnGpu or drawing anything
		void CullInstancesOnGpu(class RenderPassContext& ctx);		// call this after PrepareForRendering
		void OnForwardPassDraw(class RenderPassContext& ctx, bool useTiledLighting);	// call after CullInstancesOnGpu
		void OnGBufferPassDraw(class RenderPassContext& ctx);		// ^^
		void OnShadowMapDraw(class RenderPassContext& ctx, const RenderTargetInfo& target, glm::mat4 shadowMatrix);		// draw opaques depth-only
		void OnDrawEnd(class RenderPassContext& ctx);				// call once all drawing is complete
		void SetTiledLightinMetadataAddress(VkDeviceAddress addr) { m_lightTileMetadata = addr; }
	private:
		struct GlobalConstants;
		Frustum GetMainCameraFrustum();
		void RebuildStaticMaterialOverrides();						// re-allocate material indexes for all static material overrides + upload them to gpu. Call before RebuildInstances!
		// build instance data for a mesh component type
		template<class MeshCmpType, bool UseInterpolatedTransforms>
		void RebuildInstances(LinearWriteOnlyGpuArray<MeshInstance>& instanceBuffer, MeshPartInstanceBucket& opaques, MeshPartInstanceBucket& transparents);
		void RebuildStaticScene();									// collect static entities, rebuilds static draw buckets
		void RebuildDynamicScene();
		void PrepareDrawBucket(const MeshPartInstanceBucket& bucket, MeshPartBucketDrawIndirects& drawData);	// write draw indirects with no culling, only used when culling disabled
		void PrepareAndCullDrawBucketCompute(Device&, VkCommandBuffer cmds, const Frustum& f, VkDeviceAddress instanceDataBuffer, const MeshPartInstanceBucket& bucket, MeshPartBucketDrawIndirects& drawData);	// cull instances + write draw indirects
		bool ShowGui();
		bool CollectInstances();									// collects dynamic instances + rebuilds static scene if required. Called from frame graph
		void Cleanup(Device&);
		bool CreatePipelineLayout(Device&);
		bool CreateForwardPipelineData(Device&, VkFormat mainColourFormat, VkFormat mainDepthFormat);
		bool CreateGBufferPipelineData(Device&, VkFormat positionMetalFormat, VkFormat normalRoughnessFormat, VkFormat albedoAOFormat, VkFormat mainDepthFormat);
		bool CreateShadowPipelineData(Device&, VkFormat depthBufer);
		void OnModelReady(const ModelDataHandle& handle);	// called when a model is ready to draw

		struct FrameStats {
			uint32_t m_totalPartInstances = 0;
			uint32_t m_totalOpaqueInstances = 0;
			uint32_t m_totalTransparentInstances = 0;
			uint32_t m_totalStaticInstances = 0;
			uint32_t m_totalDynamicInstances = 0;
			double m_writeGBufferCmdsStartTime = 0.0;
			double m_writeGBufferCmdsEndTime = 0.0;
			double m_writeForwardCmdsStartTime = 0.0;
			double m_writeForwardCmdsEndTime = 0.0;
			double m_collectInstancesStartTime = 0.0;
			double m_collectInstancesEndTime = 0.0;
			double m_prepareBucketsStartTime = 0.0;
			double m_prepareBucketsEndTime = 0.0;
		};

		FrameStats m_frameStats;

		uint64_t m_onModelDataLoadedCbToken = -1;	// trigger static scene rebuild when a model loads

		bool m_enableComputeCulling = true;		// run instance culling in compute
		bool m_showGui = false;
		std::atomic<bool> m_staticSceneRebuildRequested = false;		// trigger a scene rebuild. kept separate from m_rebuildingStaticScene so it can be called from anywhere
		bool m_rebuildingStaticScene = false;							// a scene rebuild is in progress this frame

		// light tile metadata for the next draw, used in forward render only
		VkDeviceAddress m_lightTileMetadata = 0;

		std::unique_ptr<MeshInstanceCullingCompute> m_computeCulling;

		std::unique_ptr<BufferPool> m_meshRenderBufferPool;				// pool used to allocate all buffers

		LinearWriteOnlyGpuArray<MeshMaterial> m_staticMaterialOverrides;	// all static material overrides written here on scene rebuild
		LinearWriteOnlyGpuArray<MeshInstance> m_staticMeshInstances;		// all static instance data written here on static scene rebuild
		LinearWriteOnlyGpuArray<MeshInstance> m_dynamicMeshInstances;		// all dynamic instance data written here every frame

		MeshPartInstanceBucket m_staticOpaques;								// all static opaque instances collected here on scene rebuild
		MeshPartInstanceBucket m_staticTransparents;						// all static transparent instances collected here on scene rebuild
		MeshPartInstanceBucket m_dynamicOpaques;							// all dynamic opaque instances collected here every frame
		MeshPartInstanceBucket m_dynamicTransparents;						// all dynamic transparent instances collected here every frame

		MeshPartBucketDrawIndirects m_staticOpaqueDrawData;					// draw calls generated from static opaque bucket
		MeshPartBucketDrawIndirects m_staticTransparentDrawData;			// draw calls generated from static transparents bucket
		MeshPartBucketDrawIndirects m_dynamicOpaqueDrawData;				// draw calls generated from dynamic opaque bucket
		MeshPartBucketDrawIndirects m_dynamicTransparentDrawData;			// draw calls generated from dynamic opaque bucket
		MeshPartBucketDrawIndirects m_staticSunShaderCastersDrawData;		// draw calls generated from static opaque bucket for sun shadowcasters
		MeshPartBucketDrawIndirects m_dynamicSunShaderCastersDrawData;		// draw calls generated from dynamic opaque bucket for sun shadowcasters

		const uint32_t c_maxInstances = 1024 * 256;		// max static+dynamic instances we support
		const uint32_t c_maxBuffers = 3;				// we reserve space per-frame in globals, draws + dynamic instance data. this determines how many frames to handle
		const uint32_t c_maxStaticMaterialOverrides = 1024 * 8;	// max static material overrides we support
		uint32_t m_thisFrameBuffer = 0;					// determines where to write to draw + dynamic instance data each frame

		AllocatedBuffer m_drawIndirectHostVisible;	// draw indirect entries for each instance, split into c_maxBuffers sub-buffers. populated every frame from buckets
		void* m_drawIndirectMappedPtr = nullptr;
		VkDeviceAddress m_drawIndirectBufferAddress;
		uint32_t m_currentDrawBufferOffset = 0;		// next write offset for draw data, resets each frame
		
		VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
		VkPipeline m_forwardPipeline = VK_NULL_HANDLE;
		VkPipeline m_forwardTiledPipeline = VK_NULL_HANDLE;
		VkPipeline m_gBufferPipeline = VK_NULL_HANDLE;
		VkPipeline m_shadowPipeline = VK_NULL_HANDLE;
	};
}