#include "model_data.h"
#include "core/profiler.h"
#include "core/mutex.h"
#include "core/file_io.h"
#include "core/log.h"
#include <assimp/IOSystem.hpp>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/ProgressHandler.hpp>
#include <filesystem>

namespace R3
{
	// no idea if this is correct
	glm::mat4 ToGlmMatrix(const aiMatrix4x4& m)
	{
		return {
			m.a1, m.b1, m.c1, m.d1,
			m.a2, m.b2, m.c2, m.d2,
			m.a3, m.b3, m.c3, m.d3,
			m.a4, m.b4, m.c4, m.d4,
		};
	}

	void CalculateAABB(const ModelData& m, glm::vec3& minb, glm::vec3& maxb)
	{
		R3_PROF_EVENT();
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

	void ProcessMesh(const aiScene* scene, const struct aiMesh* mesh, ModelData& model, glm::mat4 transform)
	{
		R3_PROF_EVENT();

		// Make sure its triangles!
		if (!(mesh->mPrimitiveTypes & aiPrimitiveType_TRIANGLE))
		{
			LogError("Can't handle this primitive type, sorry! Triangles only");
			return;
		}

		// Process vertices
		glm::vec3 boundsMin(FLT_MAX);
		glm::vec3 boundsMax(FLT_MIN);

		Mesh newMesh;
		newMesh.m_transform = transform;
		newMesh.m_vertexDataOffset = static_cast<uint32_t>(model.m_vertices.size());
		newMesh.m_vertexCount = mesh->mNumVertices;
		MeshVertex newVertex;
		for (uint32_t v = 0; v < mesh->mNumVertices; ++v)
		{
			auto uv0 = (mesh->mTextureCoords[0] != nullptr) ? glm::vec2(mesh->mTextureCoords[0][v].x, mesh->mTextureCoords[0][v].y) : glm::vec2(0, 0);
			auto normalv0 = (mesh->mNormals != nullptr) ? glm::vec4(mesh->mNormals[v].x, mesh->mNormals[v].y, mesh->mNormals[v].z, uv0.y) 
				: glm::vec4(0, 0, 0, uv0.y);
			auto posu0 = glm::vec4(mesh->mVertices[v].x, mesh->mVertices[v].y, mesh->mVertices[v].z, uv0.x);
			auto tangentPadding = (mesh->mTangents != nullptr) ? glm::vec4(mesh->mTangents[v].x, mesh->mTangents[v].y, mesh->mTangents[v].z, 0) : glm::vec4(0);
			memcpy(newVertex.m_positionU0, glm::value_ptr(posu0), sizeof(float) * 4);
			memcpy(newVertex.m_normalV0, glm::value_ptr(normalv0), sizeof(float) * 4);
			memcpy(newVertex.m_tangentPad, glm::value_ptr(tangentPadding), sizeof(float) * 3);
			boundsMin = glm::min(boundsMin, glm::vec3(posu0));
			boundsMax = glm::max(boundsMax, glm::vec3(posu0));
			model.m_vertices.push_back(newVertex);
		}
		newMesh.m_boundsMin = boundsMin;
		newMesh.m_boundsMax = boundsMax;
		
		int indexCount = 0;
		newMesh.m_indexDataOffset = static_cast<uint32_t>(model.m_indices.size());
		model.m_indices.reserve(model.m_indices.size() + mesh->mNumFaces * 3);
		for (uint32_t face = 0; face < mesh->mNumFaces; ++face)
		{
			for (uint32_t faceIndex = 0; faceIndex < mesh->mFaces[face].mNumIndices; ++faceIndex)
			{
				++indexCount;
				// vert index is relative to the mesh part! make it relative to the entire model
				uint32_t meshVertexIndex = mesh->mFaces[face].mIndices[faceIndex];
				meshVertexIndex += newMesh.m_vertexDataOffset;
				model.m_indices.push_back(meshVertexIndex);
			}
		}
		newMesh.m_indexCount = indexCount;

		// Process materials
		if (mesh->mMaterialIndex >= 0)
		{
			newMesh.m_materialIndex = mesh->mMaterialIndex;
		}
		else
		{
			newMesh.m_materialIndex = -1;
		}

		model.m_meshes.push_back(std::move(newMesh));
	}

	void ParseSceneNode(const aiScene* scene, const aiNode* node, ModelData& model, glm::mat4 parentTransform)
	{
		R3_PROF_EVENT();
		glm::mat4 nodeTransform = ToGlmMatrix(node->mTransformation) * parentTransform;

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

	bool ParseMaterials(const aiScene* scene, ModelData& result)
	{
		R3_PROF_EVENT();
		if (scene->HasMaterials())
		{
			result.m_materials.reserve(scene->mNumMaterials);
			for (unsigned int i = 0; i < scene->mNumMaterials; ++i)
			{
				MeshMaterial newMaterial;
				const aiMaterial* sceneMat = scene->mMaterials[i];
				uint32_t diffuseTextureCount = sceneMat->GetTextureCount(aiTextureType_DIFFUSE);	// diffuse
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

				aiColor3D albedo(0.f, 0.f, 0.f);
				float opacity = 1.0f, metallic = 0.0f, roughness = 0.0f;
				sceneMat->Get(AI_MATKEY_COLOR_DIFFUSE, albedo);			// maybe use AI_MATKEY_BASE_COLOR
				sceneMat->Get(AI_MATKEY_OPACITY, opacity);
				sceneMat->Get(AI_MATKEY_METALLIC_FACTOR, metallic);		
				sceneMat->Get(AI_MATKEY_ROUGHNESS_FACTOR, roughness);
				newMaterial.m_albedo = { albedo.r,albedo.g,albedo.b };
				newMaterial.m_opacity = opacity;
				newMaterial.m_metallic = metallic;
				newMaterial.m_roughness = roughness;
				result.m_materials.push_back(std::move(newMaterial));
			}
		}
		return true;
	}

	class ModelDataAssimpProgressHandler : public Assimp::ProgressHandler
	{
	public:
		ModelDataAssimpProgressHandler(ProgressCb cb) : m_cb(cb) {}
		virtual bool Update(float progress)
		{
			if (m_cb)
			{
				m_cb(static_cast<int>(progress * 100.0f));
			}
			return true;
		}
		ProgressCb m_cb;
	};

	bool LoadModelDataAssimp(std::string_view filePath, ModelData& result, bool flattenMeshes, ProgressCb progCb)
	{
		R3_PROF_EVENT();
		// We push the root directory of the file path to assimp IO so any child files are loaded relative to the root
		auto absolutePath = std::filesystem::absolute(filePath);
		if (!absolutePath.has_filename())
		{
			LogError("Invalid path {}", absolutePath.string());
			return false;
		}
		Assimp::Importer importer;
		importer.SetProgressHandler(new ModelDataAssimpProgressHandler(progCb));	// will be deleted by assimp
		const aiScene* scene = importer.ReadFile(absolutePath.string(),
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
			LogError("Failed to load model - {}", importer.GetErrorString());
			return false;
		}
		{
			R3_PROF_EVENT("Parse");
			glm::mat4 nodeTransform = ToGlmMatrix(scene->mRootNode->mTransformation);
			ParseMaterials(scene, result);
			ParseSceneNode(scene, scene->mRootNode, result, nodeTransform);
			CalculateAABB(result, result.m_boundsMin, result.m_boundsMax);
		}
		return true;
	}

	bool LoadModelData(std::string_view filePath, ModelData& result, bool flattenMeshes, ProgressCb progCb)
	{
		char debugName[1024] = { '\0' };
		sprintf_s(debugName, "LoadModelData %s", filePath.data());
		R3_PROF_EVENT_DYN(debugName);
		return LoadModelDataAssimp(filePath, result, flattenMeshes, progCb);
	}
}