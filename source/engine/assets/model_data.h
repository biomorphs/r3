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
	struct ModelVertex
	{
		float m_positionU0[4];
		float m_normalV0[4];
		float m_tangentPad[4];
	};

	struct BakedModelVertexPosUV
	{
		float m_position[3];
		uint16_t m_uv0[2];
	};

	struct BakedModelVertexNormalTangent
	{
		uint16_t m_normal[3];
		uint16_t m_tangent[3];
	};

	struct ModelMaterial
	{
		std::vector<std::string> m_diffuseMaps;
		std::vector<std::string> m_normalMaps;
		std::vector<std::string> m_metalnessMaps;
		std::vector<std::string> m_roughnessMaps;
		std::vector<std::string> m_aoMaps;
		std::vector<std::string> m_heightMaps;
		glm::vec3 m_albedo;
		float m_opacity;
		float m_metallic;
		float m_roughness;
	};

	struct ModelPart
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
		std::vector<ModelVertex> m_vertices;	// verts are in mesh space, only stored for non-baked models
		std::vector<BakedModelVertexPosUV> m_bakedVerticesPosUV;	// verts are in mesh space + quantised
		std::vector<BakedModelVertexNormalTangent> m_bakedVerticesNormTan;	// verts are in mesh space + quantised
		std::vector<uint32_t> m_indices;
		std::vector<ModelMaterial> m_materials;
		std::vector<ModelPart> m_parts;
		glm::vec3 m_boundsMin = glm::vec3{ -1.0f };
		glm::vec3 m_boundsMax = glm::vec3{ 1.0f };
	};

	std::string GetBakedModelPath(std::string_view pathName);

	using ProgressCb = std::function<void(int)>;	// load/bake progress callback (0-100%)

	bool BakeModel(std::string_view filePath, ProgressCb progCb);
	
	bool LoadModelData(std::string_view filePath, ModelData& result, ProgressCb progCb = {});
}