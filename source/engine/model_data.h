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
		glm::vec3 m_diffuseColour;
		float m_opacity;
	};

	struct Mesh
	{
		std::vector<MeshVertex> m_vertices;	// verts are in mesh space
		std::vector<uint32_t> m_indices;
		int m_materialIndex;		// -1 = no material
		glm::mat4 m_transform;		// relative to the model
		glm::vec3 m_boundsMin;		// bounds are in mesh space
		glm::vec3 m_boundsMax;
	};

	struct ModelData
	{
		void GetGeometryMetrics(uint32_t& totalVerts, uint32_t& totalIndices) const;	// total verts/indices in all lods/parts
		std::vector<MeshMaterial> m_materials;
		std::vector<Mesh> m_meshes;
		glm::vec3 m_boundsMin = glm::vec3{ -1.0f };
		glm::vec3 m_boundsMax = glm::vec3{ 1.0f };
	};

	// flattenMeshes - flattens mesh heirarchy into an array of meshes
	bool LoadModelData(std::string_view filePath, ModelData& result, bool flattenMeshes = true);
}