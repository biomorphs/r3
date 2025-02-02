#include "static_mesh_instance_culling_compute.h"
#include "engine/systems/static_mesh_system.h"
#include "engine/systems/mesh_renderer.h"
#include "engine/utils/frustum.h"
#include "render/device.h"
#include "core/profiler.h"
#include "core/log.h"

namespace R3
{
	struct MeshInstanceCullingCompute::CullingTaskInfo
	{
		VkDeviceAddress m_drawIndirects;		// offset into allocated space already, write from index 0 to m_bucketPartInstanceCount
		VkDeviceAddress m_thisFrustum;			// offset into buffer already, only read allFrustums[0]
		VkDeviceAddress m_allBucketInstances;	// already offset into the bucket instances, read from index 0 to m_bucketPartInstanceCount
		VkDeviceAddress m_allMeshParts;			// base address
		VkDeviceAddress m_allPerDrawInstances;	// base address
		uint32_t m_bucketPartInstanceCount;		// num draw indirects to generate
	};

	struct PushConstants
	{
		VkDeviceAddress m_taskInfoDeviceAddress;	// offset into m_cullingTasksGpu for a task
	};

	MeshInstanceCullingCompute::MeshInstanceCullingCompute()
	{
	}

	MeshInstanceCullingCompute::~MeshInstanceCullingCompute()
	{
	}

	uint32_t MeshInstanceCullingCompute::UploadFrustum(const Frustum& f)	// returns offset index into the frustum buffer
	{
		R3_PROF_EVENT();
		if ((m_currentFrustumOffset + 1) < c_maxFrustums)
		{
			uint32_t newIndex = (m_currentFrustumBuffer * c_maxFrustums) + (m_currentFrustumOffset++);
			FrustumGpu newFrustum;
			for (uint32_t p = 0; p < 6; ++p)
			{
				newFrustum.m_planes[p] = f.GetPlane(p);
			}
			m_frustumsGpu.Write(newIndex, 1, &newFrustum);
			return newIndex;
		}
		else
		{
			LogError("Max frustums hit! Increase c_maxFrustums");
			return -1;
		}
	}

	uint32_t MeshInstanceCullingCompute::UploadBucketInstances(const MeshPartInstanceBucket& instanceBucket)
	{
		R3_PROF_EVENT();
		if ((m_currentBucketPartOffset + instanceBucket.m_partInstances.size()) < c_maxBucketPartInstances)
		{
			uint32_t newIndex = (m_currentBucketPartBuffer * c_maxBucketPartInstances) + m_currentBucketPartOffset;
			m_bucketPartInstancesGpu.Write(newIndex, instanceBucket.m_partInstances.size(), instanceBucket.m_partInstances.data());
			m_currentBucketPartOffset += (uint32_t)instanceBucket.m_partInstances.size();
			return newIndex;
		}
		else
		{
			LogError("Max bucket instances hit! Increase c_maxBucketPartInstances");
			return -1;
		}
	}

	void MeshInstanceCullingCompute::Reset()
	{
		R3_PROF_EVENT();
		m_currentBucketPartOffset = 0;
		if (++m_currentBucketPartBuffer >= c_maxBucketPartBuffers)
		{
			m_currentBucketPartBuffer = 0;
		}

		m_currentFrustumOffset = 0;
		if (++m_currentFrustumBuffer >= c_maxFrustumBuffers)
		{
			m_currentFrustumBuffer = 0;
		}

		m_currentCullingTaskOffset = 0;
		if (++m_currentCullingTaskBuffer >= c_maxCullingBuffers)
		{
			m_currentCullingTaskBuffer = 0;
		}
	}

	void MeshInstanceCullingCompute::Run(Device& d, VkCommandBuffer cmds, VkDeviceAddress instanceBuffer, VkDeviceAddress drawIndirectBuffer, const MeshPartInstanceBucket& instanceBucket, const MeshPartBucketDrawIndirects& draws, const Frustum& f)
	{
		R3_PROF_EVENT();
		if (draws.m_drawCount == 0)
		{
			return;
		}
		if (!m_resourcesInitialised)
		{
			if (!Initialise(d))
			{
				LogError("Failed to initialise instance culling compute!");
				return;
			}
		}

		auto staticMeshes = Systems::GetSystem<StaticMeshSystem>();

		// Upload the frustums, bucket instances, and culling task info for this job
		// We probably want to upload all this in advance and then do a single flush for all compute jobs
		// (we can then dispatch multiple culling shaders at once)
		uint32_t frustumDataIndex = UploadFrustum(f);
		VkDeviceAddress frustumBufferAddress = m_frustumsGpu.GetDataDeviceAddress() + (frustumDataIndex * sizeof(FrustumGpu));

		uint32_t bucketInstancesIndex = UploadBucketInstances(instanceBucket);
		VkDeviceAddress bucketInstancesAddress = m_bucketPartInstancesGpu.GetDataDeviceAddress() + (bucketInstancesIndex * sizeof(BucketPartInstance));

		if (frustumDataIndex == -1 || bucketInstancesIndex == -1)
		{
			LogError("Invalid buffer indexes for culling");
			return;
		}
		if (m_currentCullingTaskOffset >= c_maxCullingTasks)
		{
			LogError("Max culling tasks hit! Increase c_maxCullingTasks");
			return;
		}

		uint32_t currentCullingTaskOffset = (m_currentCullingTaskBuffer * c_maxCullingTasks) + m_currentCullingTaskOffset;
		CullingTaskInfo thisJob;
		thisJob.m_allBucketInstances = bucketInstancesAddress;
		thisJob.m_drawIndirects = drawIndirectBuffer + (draws.m_firstDrawOffset * sizeof(VkDrawIndexedIndirectCommand));
		thisJob.m_thisFrustum = frustumBufferAddress;
		thisJob.m_allMeshParts = staticMeshes->GetMeshPartsDeviceAddress();
		thisJob.m_allPerDrawInstances = instanceBuffer;
		thisJob.m_bucketPartInstanceCount = (uint32_t)instanceBucket.m_partInstances.size();
		m_cullingTasksGpu.Write(currentCullingTaskOffset, 1, &thisJob);

		m_frustumsGpu.Flush(d, cmds, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
		m_bucketPartInstancesGpu.Flush(d, cmds, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
		m_cullingTasksGpu.Flush(d, cmds, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

		PushConstants pc;
		pc.m_taskInfoDeviceAddress = m_cullingTasksGpu.GetDataDeviceAddress() + (currentCullingTaskOffset * sizeof(CullingTaskInfo));
		vkCmdBindPipeline(cmds, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipeline);
		vkCmdPushConstants(cmds, m_pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pc), &pc);
		vkCmdDispatch(cmds, (uint32_t)glm::ceil(instanceBucket.m_partInstances.size() / 64.0f), 1, 1);

		// we need a barrier here on the draw indirect buffer
		VulkanHelpers::DoMemoryBarrier(cmds, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT, VK_ACCESS_INDIRECT_COMMAND_READ_BIT);

		m_currentCullingTaskOffset++;
	}

	bool MeshInstanceCullingCompute::Initialise(Device& d)
	{
		R3_PROF_EVENT();
		m_bucketPartInstancesGpu.SetDebugName("Compute culling bucket part instances");
		if (!m_bucketPartInstancesGpu.Create(d, c_maxBucketPartInstances * c_maxBucketPartBuffers, c_maxBucketPartInstances, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT))
		{
			LogError("Failed to create bucket part instance buffer");
			return false;
		}
		m_bucketPartInstancesGpu.Allocate(c_maxBucketPartInstances * c_maxBucketPartBuffers);

		m_frustumsGpu.SetDebugName("Compute culling frustums");
		if (!m_frustumsGpu.Create(d, c_maxFrustums * c_maxFrustumBuffers, c_maxFrustums, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT))
		{
			LogError("Failed to create frustum buffer");
			return false;
		}
		m_frustumsGpu.Allocate(c_maxFrustums * c_maxFrustumBuffers);

		m_cullingTasksGpu.SetDebugName("Compute culling task info");
		if (!m_cullingTasksGpu.Create(d, c_maxCullingTasks * c_maxCullingBuffers, c_maxCullingTasks, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT))
		{
			LogError("Failed to create culling task info buffer");
			return false;
		}
		m_cullingTasksGpu.Allocate(c_maxCullingTasks * c_maxCullingBuffers);

		VkPushConstantRange constantRange;
		constantRange.offset = 0;	// needs to match in the shader if >0!
		constantRange.size = sizeof(PushConstants);
		constantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

		VkPipelineLayoutCreateInfo pipelineLayoutInfo = { 0 };
		pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutInfo.pushConstantRangeCount = 1;
		pipelineLayoutInfo.pPushConstantRanges = &constantRange;
		if (!VulkanHelpers::CheckResult(vkCreatePipelineLayout(d.GetVkDevice(), &pipelineLayoutInfo, nullptr, &m_pipelineLayout)))
		{
			LogError("Failed to create pipeline layout!");
			return false;
		}

		auto computeShader = VulkanHelpers::LoadShaderModule(d.GetVkDevice(), "shaders_spirv/common/mesh_prep_and_cull_instances_compute.comp.spv");
		if (computeShader == VK_NULL_HANDLE)
		{
			LogError("Failed to load culling shader");
			return false;
		}

		m_pipeline = VulkanHelpers::CreateComputePipeline(d.GetVkDevice(), computeShader, m_pipelineLayout, "main");
		if (m_pipeline == VK_NULL_HANDLE)
		{
			LogError("Failed to create culling compute pipeline");
			return false;
		}
		vkDestroyShaderModule(d.GetVkDevice(), computeShader, nullptr);

		m_resourcesInitialised = true;
		return true;
	}

	void MeshInstanceCullingCompute::Cleanup(Device& d)
	{
		R3_PROF_EVENT();
		vkDestroyPipeline(d.GetVkDevice(), m_pipeline, nullptr);
		vkDestroyPipelineLayout(d.GetVkDevice(), m_pipelineLayout, nullptr);
		m_bucketPartInstancesGpu.Destroy(d);
		m_frustumsGpu.Destroy(d);
		m_cullingTasksGpu.Destroy(d);
	}
}