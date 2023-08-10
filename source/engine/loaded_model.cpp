#include "loaded_model.h"
#include "core/profiler.h"
#include "core/file_io.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <fmt/format.h>

namespace R3
{
	// no idea if this is correct
	glm::mat4 ToVulkanMatrix(const aiMatrix4x4& m)
	{
		return {
			m.a1, m.b1, m.c1, m.d1,
			m.a2, m.b2, m.c2, m.d2,
			m.a3, m.b3, m.c3, m.d3,
			m.a4, m.b4, m.c4, m.d4,
		};
	}

	void CalculateAABB(const LoadedModel& m, glm::vec3& minb, glm::vec3& maxb)
	{
		glm::vec3 boundsMin(FLT_MAX), boundsMax(-FLT_MAX);
		for (const auto& m : m.m_meshes)
		{
			const auto& transform = m.m_transform;
			const auto& bmin = m.m_boundsMin;
			const auto& bmax = m.m_boundsMax;
			glm::vec3 points[] = {
				{bmin.x, bmin.y, bmin.z},	{bmax.x, bmin.y, bmin.z},
				{bmin.x, bmax.y, bmin.z},	{bmax.x, bmax.y, bmin.z},
				{bmin.x, bmin.y, bmax.z},	{bmax.x, bmin.y, bmax.z},
				{bmin.x, bmax.y, bmax.z},	{bmax.x, bmax.y, bmax.z},
			};
			for (int p=0;p<8;++p)
			{
				points[p] = glm::vec3(transform * glm::vec4(points[p],1.0f));
				boundsMin = glm::min(boundsMin, points[p]);
				boundsMax = glm::max(boundsMax, points[p]);
			}
		}
		minb = boundsMin;
		maxb = boundsMax;
	}

	void ProcessMesh(const aiScene* scene, const struct aiMesh* mesh, LoadedModel& model, glm::mat4 transform)
	{
		R3_PROF_EVENT();

		// Make sure its triangles!
		if (!(mesh->mPrimitiveTypes & aiPrimitiveType_TRIANGLE))
		{
			fmt::print("Can't handle this primitive type, sorry! Triangles only\n");
			return;
		}

		// Process vertices
		glm::vec3 boundsMin(FLT_MAX);
		glm::vec3 boundsMax(FLT_MIN);

		LoadedMesh newMesh;
		newMesh.m_transform = transform;
		newMesh.m_vertices.reserve(mesh->mNumVertices);
		LoadedMeshVertex newVertex;
		for (uint32_t v = 0; v < mesh->mNumVertices; ++v)
		{
			auto pos = glm::vec3(mesh->mVertices[v].x, mesh->mVertices[v].y, mesh->mVertices[v].z);
			memcpy(newVertex.m_position, glm::value_ptr(pos), sizeof(float) * 3);
			if (mesh->mNormals != nullptr)
			{
				memcpy(newVertex.m_normal, &mesh->mNormals[v].x, sizeof(float) * 3);
			}
			if (mesh->mTangents != nullptr)
			{
				memcpy(newVertex.m_tangent, &mesh->mBitangents[v].x, sizeof(float) * 3);
			}
			if (mesh->mTextureCoords[0] != nullptr)
			{
				memcpy(newVertex.m_texCoord0, &mesh->mTextureCoords[0][v].x, sizeof(float) * 2);
			}
			boundsMin = glm::min(boundsMin, pos);
			boundsMax = glm::max(boundsMax, pos);
			newMesh.m_vertices.push_back(newVertex);
		}
		newMesh.m_boundsMin = boundsMin;
		newMesh.m_boundsMax = boundsMax;

		// Process indices
		newMesh.m_indices.reserve(mesh->mNumFaces * 3);	// assuming triangles
		for (uint32_t face = 0; face < mesh->mNumFaces; ++face)
		{
			for (uint32_t faceIndex = 0; faceIndex < mesh->mFaces[face].mNumIndices; ++faceIndex)
			{
				newMesh.m_indices.push_back(mesh->mFaces[face].mIndices[faceIndex]);
			}
		}

		// Process materials
		if (mesh->mMaterialIndex >= 0)
		{
			LoadedMeshMaterial newMaterial;
			const aiMaterial* sceneMat = scene->mMaterials[mesh->mMaterialIndex];

			// diffuse
			uint32_t diffuseTextureCount = sceneMat->GetTextureCount(aiTextureType_DIFFUSE);
			for (uint32_t t = 0; t < diffuseTextureCount; ++t)
			{
				aiString texturePath;
				sceneMat->GetTexture(aiTextureType_DIFFUSE, t, &texturePath);
				newMaterial.m_diffuseMaps.push_back(texturePath.C_Str());
			}

			// normal maps
			uint32_t normalTextureCount = sceneMat->GetTextureCount(aiTextureType_NORMALS);
			for (uint32_t t = 0; t < normalTextureCount; ++t)
			{
				aiString texturePath;
				sceneMat->GetTexture(aiTextureType_NORMALS, t, &texturePath);
				newMaterial.m_normalMaps.push_back(texturePath.C_Str());
			}

			aiColor3D diffuseColour(0.f, 0.f, 0.f);
			float opacity = 1.0f;
			sceneMat->Get(AI_MATKEY_COLOR_DIFFUSE, diffuseColour);
			sceneMat->Get(AI_MATKEY_OPACITY, opacity);
			newMaterial.m_diffuseColour = { diffuseColour.r,diffuseColour.g,diffuseColour.b };
			newMaterial.m_opacity = opacity;
			newMesh.m_material = std::move(newMaterial);
		}

		model.m_meshes.push_back(std::move(newMesh));
	}

	void ParseSceneNode(const aiScene* scene, const aiNode* node, LoadedModel& model, glm::mat4 parentTransform)
	{
		glm::mat4 nodeTransform = ToVulkanMatrix(node->mTransformation) * parentTransform;

		for (uint32_t meshIndex = 0; meshIndex < node->mNumMeshes; ++meshIndex)
		{
			assert(scene->mNumMeshes >= node->mMeshes[meshIndex]);
			ProcessMesh(scene, scene->mMeshes[node->mMeshes[meshIndex]], model, nodeTransform);
		}

		for (uint32_t childIndex = 0; childIndex < node->mNumChildren; ++childIndex)
		{
			ParseSceneNode(scene, node->mChildren[childIndex], model, nodeTransform);
		}
	}

	bool LoadModel(std::string_view filePath, LoadedModel& result, bool flattenMeshes)
	{
		R3_PROF_EVENT();

		Assimp::Importer importer;
		std::string fullPath = std::string(FileIO::GetBasePath()) + std::string(filePath);
		const aiScene* scene = importer.ReadFile(fullPath,
			aiProcess_CalcTangentSpace |
			aiProcess_GenNormals |	// only if no normals in data
			aiProcess_Triangulate |
			aiProcess_JoinIdenticalVertices |
			aiProcess_SortByPType |
			aiProcess_ValidateDataStructure |
			aiProcess_OptimizeMeshes |
			aiProcess_RemoveRedundantMaterials |
			(flattenMeshes ? aiProcess_PreTransformVertices : 0)
		);
		if (!scene)
		{
			return false;
		}

		glm::mat4 nodeTransform = ToVulkanMatrix(scene->mRootNode->mTransformation);
		ParseSceneNode(scene, scene->mRootNode, result, nodeTransform);
		CalculateAABB(result, result.m_boundsMin, result.m_boundsMax);

		return true;
	}
}