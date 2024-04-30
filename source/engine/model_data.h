#pragma once

/////////////////////////////////////////////////
// A render-agnostic model containing a flat list of meshes to draw
// BLENDER FBX EXPORT SETTINGS
// PATH MODE - RELATIVE
// Apply scalings - FBX all
// Forward -z forward
// Up Y up
// Apply unit + transformation

#include "core/glm_headers.h"
#include <vector>
#include <string>
#include <memory>
#include <functional>

struct aiNode;
struct aiScene;
struct aiMesh;

namespace R3
{
	struct MeshVertex
	{
		float m_positionU0[4];
		float m_normalV0[4];
		float m_tangentPad[4];
	};

	struct MeshMaterial
	{
		std::vector<std::string> m_diffuseMaps;
		std::vector<std::string> m_normalMaps;
		glm::vec3 m_albedo;
		float m_opacity;
		float m_metallic;
		float m_roughness;
	};

	struct Mesh
	{
		glm::mat4 m_transform;		// relative to the model
		glm::vec3 m_boundsMin;		// bounds are in mesh space
		glm::vec3 m_boundsMax;
		uint32_t m_vertexDataOffset;
		uint32_t m_vertexCount;
		uint32_t m_indexDataOffset;
		uint32_t m_indexCount;
		int m_materialIndex;		// -1 = no material
	};

	struct ModelData
	{
		std::vector<MeshVertex> m_vertices;	// verts are in mesh space
		std::vector<uint32_t> m_indices;
		std::vector<MeshMaterial> m_materials;
		std::vector<Mesh> m_meshes;
		glm::vec3 m_boundsMin = glm::vec3{ -1.0f };
		glm::vec3 m_boundsMax = glm::vec3{ 1.0f };
	};

	// flattenMeshes - flattens mesh heirarchy into an array of meshes
	using ProgressCb = std::function<void(int)>;
	bool LoadModelData(std::string_view filePath, ModelData& result, bool flattenMeshes = true, ProgressCb progCb = {});
}