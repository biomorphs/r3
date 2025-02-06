#pragma once
#include "core/glm_headers.h"
#include "render/vulkan_helpers.h"
#include "render/writeonly_gpu_buffer.h"
#include <memory>

namespace R3
{
	class Device;
	struct RenderTarget;
	struct MeshPartInstanceBucket;
	struct MeshPartBucketDrawIndirects;
	struct BucketPartInstance;
	class Frustum;
	class MeshInstanceCullingCompute
	{
	public:
		MeshInstanceCullingCompute();
		~MeshInstanceCullingCompute();

		void Reset();	// call at the start or end of each frame
		void Run(Device& d, VkCommandBuffer cmds, VkDeviceAddress instanceBuffer, VkDeviceAddress drawIndirectBuffer, const MeshPartInstanceBucket& instanceBucket, const MeshPartBucketDrawIndirects& draws,  const Frustum& f);
		bool Initialise(Device& d);
		void Cleanup(Device& d);

	private:
		uint32_t UploadFrustum(const Frustum& f);	// returns absoluet offset index into the frustum buffer
		uint32_t UploadBucketInstances(const MeshPartInstanceBucket& instanceBucket);

		// track if we need to initialise internal state
		bool m_resourcesInitialised = false;

		VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
		VkPipeline m_pipeline = VK_NULL_HANDLE;

		WriteOnlyGpuArray<BucketPartInstance> m_bucketPartInstancesGpu;	// bucket instance data must be uploaded to gpu each frame before culling
		const uint32_t c_maxBucketPartInstances = 1024 * 256;
		const uint32_t c_maxBucketPartBuffers = 3;
		uint32_t m_currentBucketPartBuffer = 0;		// 0 to c_maxBucketPartBuffers
		uint32_t m_currentBucketPartOffset = 0;		// 0 to c_maxBucketPartInstances

		struct FrustumGpu
		{
			glm::vec4 m_planes[6];
		};
		WriteOnlyGpuArray<FrustumGpu> m_frustumsGpu;	// frustums uploaded each frame before culling
		const uint32_t c_maxFrustums = 10;
		const uint32_t c_maxFrustumBuffers = 3;
		uint32_t m_currentFrustumBuffer = 0;		// 0 to c_maxFrustumBuffers
		uint32_t m_currentFrustumOffset = 0;		// 0 to c_maxFrustums

		struct CullingTaskInfo;
		WriteOnlyGpuArray<CullingTaskInfo> m_cullingTasksGpu;	// one per culling job per frame
		const uint32_t c_maxCullingTasks = 10;
		const uint32_t c_maxCullingBuffers = 3;
		uint32_t m_currentCullingTaskBuffer = 0;	// 0 to c_maxCullingBuffers
		uint32_t m_currentCullingTaskOffset = 0;	// 0 to c_maxCullingTasks
	};
}