#pragma once 
#include "engine/systems.h"
#include "render/vulkan_helpers.h"
#include "render/writeonly_gpu_buffer.h"
#include "core/glm_headers.h"
#include "core/callback_array.h"
#include "core/mutex.h"

// This system handles only STATIC data associated with meshes
// vertices/indices/materials/parts
// cpu and gpu versions are kept for convenience
namespace R3
{
	namespace Entities
	{
		class EntityHandle;
	}

	struct MeshPart								// uploaded to GPU in m_allParts
	{
		glm::mat4 m_transform;					// relative to the model
		glm::vec4 m_boundsMin;					// mesh space bounds (w unused)
		glm::vec4 m_boundsMax;
		uint32_t m_indexStartOffset;			// offset into indices (index data is local to the mesh, verts must be offset by mesh vertex offset)
		uint32_t m_indexCount;					//
		uint32_t m_materialIndex;				// index into mesh data material array
		uint32_t m_vertexDataOffset;			// used when generating draw calls
	};

	struct MeshMaterial							// uploaded to GPU in m_allMaterialsGpu
	{
		glm::vec4 m_albedoOpacity;
		glm::vec4 m_uvOffsetScale = { 0,0,1,1 };		// uv offset/scale, useful for custom materials
		float m_metallic = 0.0f;						// 0.0 = dielectric, 1 = metallic
		float m_roughness = 0.25f;						// 0 = perfectly smooth, 1 = max roughness
		uint32_t m_albedoTexture = -1;					// index into global textures, -1 = no texture
		uint32_t m_roughnessTexture = -1;
		uint32_t m_metalnessTexture = -1;
		uint32_t m_normalTexture = -1;
		uint32_t m_aoTexture = -1;
	};

	struct MeshDrawData
	{
		glm::vec3 m_boundsMin;					// mesh space bounds
		glm::vec3 m_boundsMax;
		uint32_t m_vertexDataOffset;			// one giant vertex buffer
		uint32_t m_indexDataOffset;				// one giant index buffer
		uint32_t m_materialGpuIndex;			// index into the array of materials (cpu and gpu!)
		uint32_t m_firstMeshPartOffset;			// one big buffer
		uint32_t m_meshPartCount;
		uint32_t m_modelHandleIndex;			// used to identify which model data this came from
		uint32_t m_totalVertices;
		uint32_t m_totalIndices;
		uint32_t m_materialCount;
	};

	struct ModelVertex;
	struct ModelDataHandle;
	class StaticMeshSystem : public System
	{
	public:
		StaticMeshSystem();
		virtual ~StaticMeshSystem();
		static std::string_view GetName() { return "StaticMeshes"; }
		virtual void RegisterTickFns();
		virtual bool Init();
		virtual void Shutdown();
		void PrepareForRendering(class RenderPassContext& ctx);		// call from frame graph before drawing anything

		VkDeviceAddress GetVertexDataDeviceAddress();
		VkDeviceAddress GetMeshPartsDeviceAddress();
		VkDeviceAddress GetMaterialsDeviceAddress();
		VkBuffer GetIndexBuffer();

		bool GetMeshDataForModel(const ModelDataHandle& handle, MeshDrawData& result);
		bool GetMeshPart(uint32_t partIndex, MeshPart& result);
		bool GetMeshMaterial(uint32_t materialIndex, MeshMaterial& result);
		const MeshMaterial* GetMeshMaterial(uint32_t materialIndex);
		const MeshPart* GetMeshPart(uint32_t partIndex);

		// Callbacks that fire when a model is ready to draw
		using ModelReadyCallback = std::function<void(const ModelDataHandle&)>;
		using ModelReadyCallbacks = CallbackArray<ModelReadyCallback>;
		ModelReadyCallbacks::Token RegisterModelReadyCallback(const ModelReadyCallback& fn);
		bool UnregisterModelReadyCallback(ModelReadyCallbacks::Token token);

	private:
		void OnModelDataLoaded(const ModelDataHandle& handle, bool loaded);
		bool PrepareForUpload(const ModelDataHandle& handle);	// returns true if already uploaded or ready to go 
		bool ShowGui();
		
		bool m_showGui = false;
		uint64_t m_onModelDataLoadedCbToken = -1;

		ModelReadyCallbacks m_onModelReadyCallbacks;				// called when a model is fully uploaded and ready to draw

		Mutex m_allDataMutex;	// protects stuff below
		std::vector<MeshDrawData> m_allData;
		std::vector<MeshMaterial> m_allMaterials;				// cpu-side copy of m_allMaterialsGpu
		std::vector<MeshPart> m_allParts;						// cpu-side copy of m_allMeshPartsGpu

		WriteOnlyGpuArray<MeshMaterial> m_allMaterialsGpu;	// gpu buffer of materials
		WriteOnlyGpuArray<MeshPart> m_allMeshPartsGpu;		// gpu buffer of mesh parts
		WriteOnlyGpuArray<ModelVertex> m_allVertices;
		WriteOnlyGpuArray<uint32_t> m_allIndices;

		const uint32_t c_maxVerticesToStore = 1024 * 1024 * 16;		// ~800mb
		const uint32_t c_maxIndicesToStore = 1024 * 1024 * 32;		// ~128mb
		const uint32_t c_maxMaterialsToStore = 1024 * 32;			// ~2mb
		const uint32_t c_maxMeshParts = 1024 * 32;					// ~3.5mb
	};
}