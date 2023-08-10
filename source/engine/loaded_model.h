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
	struct LoadedMeshVertex
	{
		float m_position[3];
		float m_normal[3];
		float m_tangent[3];
		float m_texCoord0[2];
	};

	struct LoadedMeshMaterial
	{
		std::vector<std::string> m_diffuseMaps;
		std::vector<std::string> m_normalMaps;
		glm::vec3 m_diffuseColour;
		float m_opacity;
	};

	struct LoadedMesh
	{
		std::vector<LoadedMeshVertex> m_vertices;	// verts are in mesh space
		std::vector<uint32_t> m_indices;
		int m_materialIndex;		// -1 = no material
		glm::mat4 m_transform;		// relative to the model
		glm::vec3 m_boundsMin;		// bounds are in mesh space
		glm::vec3 m_boundsMax;
	};

	struct LoadedModel
	{
		std::vector<LoadedMeshMaterial> m_materials;
		std::vector<LoadedMesh> m_meshes;
		glm::vec3 m_boundsMin;
		glm::vec3 m_boundsMax;
	};

	// flattenMeshes - flattens mesh heirarchy into an array of meshes
	bool LoadModel(std::string_view filePath, LoadedModel& result, bool flattenMeshes = true);
}