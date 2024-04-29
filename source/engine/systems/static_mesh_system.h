#pragma once 

#include "engine/systems.h"
#include "engine/model_data_handle.h"
#include "render/vulkan_helpers.h"
#include "render/writeonly_gpu_buffer.h"
#include "core/glm_headers.h"
#include "core/mutex.h"
#include <concurrentqueue/concurrentqueue.h>

namespace R3
{
	struct MeshVertex;
	class StaticMeshSystem : public System
	{
	public:
		StaticMeshSystem();
		virtual ~StaticMeshSystem();
		static std::string_view GetName() { return "StaticMeshes"; }
		virtual void RegisterTickFns();
		virtual bool Init();
		virtual void Shutdown();
	private:
		void OnModelDataLoaded(const ModelDataHandle& handle, bool loaded);
		bool PrepareForUpload(const ModelDataHandle& handle);	// returns true if already uploaded or ready to go 
		void OnMainPassBegin(class Device& d, VkCommandBuffer cmds);	// Render hooks
		bool ShowGui();
		
		uint64_t m_onModelDataLoadedCbToken = -1;
		uint64_t m_onMainPassBeginToken = -1;

		struct StaticMeshPart
		{
			glm::vec3 m_boundsMin;					// mesh space bounds
			glm::vec3 m_boundsMax;
			uint64_t m_vertexStartOffset;			// offset into verts
			uint64_t m_indexStartOffset;			// offset into indices
			uint32_t m_vertexCount;
			uint32_t m_indexCount;					//
			uint32_t m_materialIndex;				// index into mesh data material array
		};
		struct StaticMeshMaterial
		{
			glm::vec4 m_diffuseOpacity;
		};
		struct StaticMeshGpuData
		{
			glm::vec3 m_boundsMin;					// mesh space bounds
			glm::vec3 m_boundsMax;
			uint64_t m_vertexDataOffset;			// one giant vertex buffer
			uint64_t m_indexDataOffset;				// one giant index buffer
			uint32_t m_totalVertices;
			uint32_t m_totalIndices;
			uint32_t m_firstMaterialOffset;			// one giant material buffer
			uint32_t m_materialCount;
			uint32_t m_firstMeshPartOffset;			// one big buffer
			uint32_t m_meshPartCount;
			uint32_t m_modelHandleIndex;			// used to identify which model data this came from
		};
		Mutex m_allDataMutex;	// protects stuff below
		std::vector<StaticMeshGpuData> m_allData;
		std::vector<StaticMeshMaterial> m_allMaterials;
		std::vector<StaticMeshPart> m_allParts;

		WriteOnlyGpuArray<MeshVertex> m_allVertices;
		WriteOnlyGpuArray<uint32_t> m_allIndices;
		const uint32_t c_maxVerticesToStore = 1024 * 1024 * 16;		// ~800mb
		const uint32_t c_maxIndicesToStore = 1024 * 1024 * 64;		// ~256mb
	};
}