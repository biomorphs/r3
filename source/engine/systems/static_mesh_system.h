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
	namespace Entities
	{
		class EntityHandle;
	}

	struct StaticMeshPart
	{
		glm::mat4 m_transform;					// relative to the model
		glm::vec3 m_boundsMin;					// mesh space bounds
		glm::vec3 m_boundsMax;
		uint64_t m_vertexStartOffset;			// offset into verts
		uint64_t m_indexStartOffset;			// offset into indices (index data is local to the mesh, verts must be offset by mesh vertex offset)
		uint32_t m_vertexCount;
		uint32_t m_indexCount;					//
		uint32_t m_materialIndex;				// index into mesh data material array
	};
	struct StaticMeshMaterial
	{
		glm::vec4 m_albedoOpacity;
		glm::vec4 m_uvOffsetScale = { 0,0,1,1 };		// uv offset/scale, useful for custom materials
		float m_metallic = 0.0f;						// 0.0 = dielectric, 1 = metallic
		float m_roughness = 0.25f;						// 0 = perfectly smooth, 1 = max roughness
		float m_paralaxAmount = 0.08f;					// controls 'height' when heightmap texture is available
		uint32_t m_paralaxShadowsEnabled = 0;			// if > 0, enables soft self-shadows when heightmap texture is available
		uint32_t m_albedoTexture = -1;					// index into global textures, -1 = no texture
		uint32_t m_roughnessTexture = -1;
		uint32_t m_metalnessTexture = -1;
		uint32_t m_normalTexture = -1;
		uint32_t m_aoTexture = -1;
		uint32_t m_heightmapTexture = -1;
	};
	struct StaticMeshGpuData
	{
		glm::vec3 m_boundsMin;					// mesh space bounds
		glm::vec3 m_boundsMax;
		uint64_t m_vertexDataOffset;			// one giant vertex buffer
		uint64_t m_indexDataOffset;				// one giant index buffer
		uint32_t m_totalVertices;
		uint32_t m_totalIndices;
		uint32_t m_materialArrayIndex;			// index into the cpu-side array of materials
		uint32_t m_materialGpuIndex;			// index into the gpu-side array of materials, only use on gpu!!
		uint32_t m_materialCount;
		uint32_t m_firstMeshPartOffset;			// one big buffer
		uint32_t m_meshPartCount;
		uint32_t m_modelHandleIndex;			// used to identify which model data this came from
	};

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
		void OnMainPassBegin(class RenderPassContext& ctx);
		VkDeviceAddress GetVertexDataDeviceAddress();
		VkDeviceAddress GetMaterialsDeviceAddress();
		VkBuffer GetIndexBuffer();
		bool GetMeshDataForModel(const ModelDataHandle& handle, StaticMeshGpuData& result);
		bool GetMeshPart(uint32_t partIndex, StaticMeshPart& result);
		bool GetMeshMaterial(uint32_t materialIndex, StaticMeshMaterial& result);

		// Searches the active world entities for any entities with static mesh components that intersect the ray
		// returns the closest hit entity (nearest to rayStart)
		Entities::EntityHandle FindClosestActiveEntityIntersectingRay(glm::vec3 rayStart, glm::vec3 rayEnd);

	private:
		void OnModelDataLoaded(const ModelDataHandle& handle, bool loaded);
		bool PrepareForUpload(const ModelDataHandle& handle);	// returns true if already uploaded or ready to go 
		void OnMainPassBegin(class Device& d, VkCommandBuffer cmds);	// Render hooks
		bool ShowGui();
		
		bool m_showGui = false;
		uint64_t m_onModelDataLoadedCbToken = -1;

		Mutex m_allDataMutex;	// protects stuff below
		std::vector<StaticMeshGpuData> m_allData;
		std::vector<StaticMeshMaterial> m_allMaterials;
		std::vector<StaticMeshPart> m_allParts;

		WriteOnlyGpuArray<StaticMeshMaterial> m_allMaterialsGpu;	// gpu buffer of materials
		WriteOnlyGpuArray<MeshVertex> m_allVertices;
		WriteOnlyGpuArray<uint32_t> m_allIndices;
		const uint32_t c_maxVerticesToStore = 1024 * 1024 * 16;		// ~800mb
		const uint32_t c_maxIndicesToStore = 1024 * 1024 * 64;		// ~256mb
		const uint32_t c_maxMaterialsToStore = 1024 * 128;
	};
}