#include "model_data.h"
#include "asset_file.h"
#include "core/profiler.h"
#include "core/mutex.h"
#include "core/file_io.h"
#include "core/log.h"
#include <assimp/IOSystem.hpp>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/ProgressHandler.hpp>
#include <meshoptimizer.h>
#include <filesystem>

namespace R3
{
	const uint32_t c_bakedModelVersion = 3;		// change this to force rebake
	const uint32_t c_bakedMaterialTexturePathLength = 256;	// avoid std::string in materials
	const std::string c_bakedModelExtension = ".bmdl";
	const std::string c_bakeSettingsExtension = ".bakesettings.json";

	struct BakeSettings 
	{
		// assimp settings
		bool m_flattenMeshes = false;
		bool m_splitLargeMeshes = false;
		bool m_improveCacheLocality = true;
		bool m_fixInFacingNormals = false;

		// meshoptimizer settings
		bool m_runMeshOptimizer = true;
		bool m_simplifyModel = false;				// run mesh simplification on the entire model
		float m_simplifyIndexThreshold = 0.5f;		// the desired reduction in index count (0-1.0)
		float m_simplifyTargetError = 0.005f;		// acceptable % error when simplifying the model
		float m_optimiseOverdrawThreshold = 1.05f;	// How much cache efficiency can be sacrificed to optimise for overdraw instead 
	};

	struct AssimpLoadSettings
	{
		bool m_preTransformVertices = false;		// aiProcess_PreTransformVertices
		bool m_splitLargeMeshes = false;			// aiProcess_SplitLargeMeshes
		bool m_improveCacheLocality = true;			// aiProcess_ImproveCacheLocality
		bool m_fixInFacingNormals = false;			// aiProcess_FixInfacingNormals
	};

	struct BakedMaterial						// avoid std::string in binary data, only store 1 path per texture type
	{
		char m_diffuseTexture[c_bakedMaterialTexturePathLength] = { '\0' };
		char m_normalTexture[c_bakedMaterialTexturePathLength] = { '\0' };
		char m_metalTexture[c_bakedMaterialTexturePathLength] = { '\0' };
		char m_roughnessTexture[c_bakedMaterialTexturePathLength] = { '\0' };
		char m_aoTexture[c_bakedMaterialTexturePathLength] = { '\0' };
		char m_heightTexture[c_bakedMaterialTexturePathLength] = { '\0' };
		glm::vec3 m_albedo;
		float m_opacity;
		float m_metallic;
		float m_roughness;
	};

	bool LoadBakeSettings(std::string_view modelPath, BakeSettings& target)
	{
		R3_PROF_EVENT();

		std::string bakeSettingsPath(modelPath);
		bakeSettingsPath += c_bakeSettingsExtension;

		std::string jsonText;
		if (!std::filesystem::exists(bakeSettingsPath) || !FileIO::LoadTextFromFile(bakeSettingsPath, jsonText))
		{
			return false;
		}

		auto parsedSettings = nlohmann::json::parse(jsonText);
		if(parsedSettings.contains("FlattenMeshes"))
			target.m_flattenMeshes = parsedSettings["FlattenMeshes"];
		if (parsedSettings.contains("SplitLargeMeshes"))
			target.m_splitLargeMeshes = parsedSettings["SplitLargeMeshes"];
		if (parsedSettings.contains("ImproveCacheLocality"))
			target.m_improveCacheLocality = parsedSettings["ImproveCacheLocality"];
		if (parsedSettings.contains("FixInFacingNormals"))
			target.m_fixInFacingNormals = parsedSettings["FixInFacingNormals"];
		if (parsedSettings.contains("RunMeshOptimizer"))
			target.m_runMeshOptimizer = parsedSettings["RunMeshOptimizer"];
		if (parsedSettings.contains("SimplifyModel"))
			target.m_simplifyModel = parsedSettings["SimplifyModel"];
		if (parsedSettings.contains("SimplifyIndexThreshold"))
			target.m_simplifyIndexThreshold = parsedSettings["SimplifyIndexThreshold"];
		if (parsedSettings.contains("SimplifyTargetError"))
			target.m_simplifyTargetError = parsedSettings["SimplifyTargetError"];
		if (parsedSettings.contains("OverdrawCacheThreshold"))
			target.m_simplifyTargetError = parsedSettings["OverdrawCacheThreshold"];

		return true;
	}

	void ParseBakedMaterial(const BakedMaterial& baked, ModelMaterial& result)
	{
		if (strnlen(baked.m_diffuseTexture, c_bakedMaterialTexturePathLength) != 0)
		{
			result.m_diffuseMaps.push_back(baked.m_diffuseTexture);
		}
		if (strnlen(baked.m_normalTexture, c_bakedMaterialTexturePathLength) != 0)
		{
			result.m_normalMaps.push_back(baked.m_normalTexture);
		}
		if (strnlen(baked.m_metalTexture, c_bakedMaterialTexturePathLength) != 0)
		{
			result.m_metalnessMaps.push_back(baked.m_metalTexture);
		}
		if (strnlen(baked.m_roughnessTexture, c_bakedMaterialTexturePathLength) != 0)
		{
			result.m_roughnessMaps.push_back(baked.m_roughnessTexture);
		}
		if (strnlen(baked.m_aoTexture, c_bakedMaterialTexturePathLength) != 0)
		{
			result.m_aoMaps.push_back(baked.m_aoTexture);
		}
		if (strnlen(baked.m_heightTexture, c_bakedMaterialTexturePathLength) != 0)
		{
			result.m_heightMaps.push_back(baked.m_heightTexture);
		}
		result.m_albedo = baked.m_albedo;
		result.m_metallic = baked.m_metallic;
		result.m_opacity = baked.m_opacity;
		result.m_roughness = baked.m_roughness;
	}

	bool MakeBakedMaterial(const ModelMaterial& src, BakedMaterial& target)
	{
		auto bakePath = [](const std::vector<std::string>& srcPaths, char* targetPath)
		{
			if (srcPaths.size() > 0)
			{
				if (srcPaths[0].size() > c_bakedMaterialTexturePathLength)
				{
					LogError("Texture path {} in material too long for baking!", srcPaths[0]);
					return false;
				}
				strncpy_s(targetPath, c_bakedMaterialTexturePathLength, srcPaths[0].data(), srcPaths[0].size());
			}
			return true;
		};
		if (!bakePath(src.m_diffuseMaps, target.m_diffuseTexture))
		{
			return false;
		}
		if (!bakePath(src.m_normalMaps, target.m_normalTexture))
		{
			return false;
		}
		if (!bakePath(src.m_metalnessMaps, target.m_metalTexture))
		{
			return false;
		}
		if (!bakePath(src.m_roughnessMaps, target.m_roughnessTexture))
		{
			return false;
		}
		if (!bakePath(src.m_aoMaps, target.m_aoTexture))
		{
			return false;
		}
		if (!bakePath(src.m_heightMaps, target.m_heightTexture))
		{
			return false;
		}
		target.m_albedo = src.m_albedo;
		target.m_metallic = src.m_metallic;
		target.m_opacity = src.m_opacity;
		target.m_roughness = src.m_roughness;
		return true;
	}

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
		for (const auto& m : m.m_parts)
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
			return;
		}

		// Process vertices
		glm::vec3 boundsMin(FLT_MAX);
		glm::vec3 boundsMax(FLT_MIN);

		ModelPart newMesh;
		newMesh.m_transform = transform;
		newMesh.m_vertexDataOffset = static_cast<uint32_t>(model.m_vertices.size());
		newMesh.m_vertexCount = mesh->mNumVertices;
		ModelVertex newVertex;
		for (uint32_t v = 0; v < mesh->mNumVertices; ++v)
		{
			auto uv0 = (mesh->mTextureCoords[0] != nullptr) ? glm::vec2(mesh->mTextureCoords[0][v].x, mesh->mTextureCoords[0][v].y) : glm::vec2(0, 0);
			auto normalv0 = (mesh->mNormals != nullptr) ? glm::vec4(mesh->mNormals[v].x, mesh->mNormals[v].y, mesh->mNormals[v].z, uv0.y) 
				: glm::vec4(0, 0, 0, uv0.y);
			auto posu0 = glm::vec4(mesh->mVertices[v].x, mesh->mVertices[v].y, mesh->mVertices[v].z, uv0.x);
			auto tangentPadding = (mesh->mTangents != nullptr) ? glm::vec4(mesh->mTangents[v].x, mesh->mTangents[v].y, mesh->mTangents[v].z, 0) : glm::vec4(0);
			memcpy(newVertex.m_positionU0, glm::value_ptr(posu0), sizeof(float) * 4);
			memcpy(newVertex.m_normalV0, glm::value_ptr(normalv0), sizeof(float) * 4);
			memcpy(newVertex.m_tangentPad, glm::value_ptr(tangentPadding), sizeof(float) * 4);
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

		model.m_parts.push_back(std::move(newMesh));
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

	// material texture paths are relative to the model path (or absolute, yuk)
	// we want to get a path from the data root to the texture via the model path
	std::string FixupTexturePath(const std::filesystem::path& modelPath, std::string texturePath)
	{
		auto texPath = modelPath.parent_path().append(texturePath);
		return FileIO::SanitisePath(texPath.string());
	}

	bool ParseMaterials(const std::filesystem::path& modelPathAbsolute, const aiScene* scene, ModelData& result)
	{
		R3_PROF_EVENT();
		if (scene->HasMaterials())
		{
			result.m_materials.reserve(scene->mNumMaterials);
			for (unsigned int i = 0; i < scene->mNumMaterials; ++i)
			{
				ModelMaterial newMaterial;
				const aiMaterial* sceneMat = scene->mMaterials[i];
				auto getTexture = [&](aiTextureType type, std::vector<std::string>& out) {
					uint32_t count = sceneMat->GetTextureCount(type);
					for (uint32_t t = 0; t < count; ++t)
					{
						aiString texturePath;
						sceneMat->GetTexture(type, t, &texturePath);
						out.push_back(FixupTexturePath(modelPathAbsolute, texturePath.C_Str()));
					}
				};
				getTexture(aiTextureType_BASE_COLOR, newMaterial.m_diffuseMaps);
				if (newMaterial.m_diffuseMaps.size() == 0)
				{
					getTexture(aiTextureType_DIFFUSE, newMaterial.m_diffuseMaps);
				}
				getTexture(aiTextureType_NORMALS, newMaterial.m_normalMaps);
				getTexture(aiTextureType_METALNESS, newMaterial.m_metalnessMaps);
				getTexture(aiTextureType_DIFFUSE_ROUGHNESS, newMaterial.m_roughnessMaps);
				getTexture(aiTextureType_AMBIENT_OCCLUSION, newMaterial.m_aoMaps);
				getTexture(aiTextureType_HEIGHT, newMaterial.m_heightMaps);
				getTexture(aiTextureType_DISPLACEMENT, newMaterial.m_heightMaps);	// just in case

				aiColor3D albedo(0.f, 0.f, 0.f);
				float opacity = 1.0f, metallic = 0.0f, roughness = 0.15f;
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

	bool LoadModelDataAssimp(std::string_view filePath, ModelData& result, const AssimpLoadSettings& settings, ProgressCb progCb)
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
			aiProcess_FlipUVs |
			aiProcess_RemoveRedundantMaterials |
			(settings.m_preTransformVertices ? aiProcess_PreTransformVertices : 0) |
			(settings.m_splitLargeMeshes ? aiProcess_SplitLargeMeshes : 0) |
			(settings.m_improveCacheLocality ? aiProcess_ImproveCacheLocality : 0) |
			(settings.m_fixInFacingNormals ? aiProcess_FixInfacingNormals : 0)
		);
		if (!scene)
		{
			LogError("Failed to load model - {}", importer.GetErrorString());
			return false;
		}
		{
			R3_PROF_EVENT("Parse");
			glm::mat4 nodeTransform = ToGlmMatrix(scene->mRootNode->mTransformation);
			ParseMaterials(absolutePath, scene, result);
			ParseSceneNode(scene, scene->mRootNode, result, nodeTransform);
			CalculateAABB(result, result.m_boundsMin, result.m_boundsMax);
		}
		return true;
	}

	std::string GetBakedModelPath(std::string_view pathName)
	{
		R3_PROF_EVENT();
		// get the source path relative to data base directory
		std::string relPath = FileIO::SanitisePath(pathName);
		if (relPath.size() == 0)
		{
			LogWarn("Model file {} is outside data root", pathName);
			return {};
		}

		// replace any directory separators with '_'
		std::replace(relPath.begin(), relPath.end(), '/', '_');
		std::replace(relPath.begin(), relPath.end(), '\\', '_');

		// add our own baked version + extension (so we dont need to check the version in the file)
		relPath += c_bakedModelExtension;
		relPath += std::to_string(c_bakedModelVersion);

		// use the temp directory for baked data
		std::string bakedPath = std::string(FileIO::GetBasePath()) + "\\baked\\" + relPath;
		return std::filesystem::absolute(bakedPath).string();
	}

	bool LoadBakedModel(std::string_view filePath, ModelData& result, ProgressCb progCb)
	{
		R3_PROF_EVENT();

		auto loadedFile = LoadAssetFile(filePath);
		if (!loadedFile.has_value())
		{
			LogError("Failed to load baked model file {}", filePath);
			return false;
		}
		uint32_t fileVersion = loadedFile->m_header["Version"];
		if (fileVersion != c_bakedModelVersion)
		{
			LogError("Baked model is an older version {}. Current version = {}", fileVersion, c_bakedModelVersion);
			return false;
		}
		result.m_boundsMin = {
			loadedFile->m_header["BoundsMin"]["X"],
			loadedFile->m_header["BoundsMin"]["Y"],
			loadedFile->m_header["BoundsMin"]["Z"]
		};
		result.m_boundsMax = {
			loadedFile->m_header["BoundsMax"]["X"],
			loadedFile->m_header["BoundsMax"]["Y"],
			loadedFile->m_header["BoundsMax"]["Z"]
		};
		progCb(5);
		uint32_t vertexCount = loadedFile->m_header["VertexCount"];
		auto vertexPosUVBlob = loadedFile->GetBlob("VerticesPosUV");
		if ((vertexCount * sizeof(BakedModelVertexPosUV)) == vertexPosUVBlob->m_data.size())
		{
			result.m_bakedVerticesPosUV.resize(vertexCount);
			memcpy(result.m_bakedVerticesPosUV.data(), vertexPosUVBlob->m_data.data(), vertexCount * sizeof(BakedModelVertexPosUV));
		}
		else
		{
			LogError("Unexpected vertex data size");
			return false;
		}
		auto vertexNormTanBlob = loadedFile->GetBlob("VerticesNormTan");
		if ((vertexCount * sizeof(BakedModelVertexNormalTangent)) == vertexNormTanBlob->m_data.size())
		{
			result.m_bakedVerticesNormTan.resize(vertexCount);
			memcpy(result.m_bakedVerticesNormTan.data(), vertexNormTanBlob->m_data.data(), vertexCount * sizeof(BakedModelVertexNormalTangent));
		}
		else
		{
			LogError("Unexpected vertex data size");
			return false;
		}
		progCb(40);

		uint32_t indexCount = loadedFile->m_header["IndexCount"];
		auto indexBlob = loadedFile->GetBlob("Indices");
		if ((indexCount * sizeof(uint32_t)) == indexBlob->m_data.size())
		{
			result.m_indices.resize(indexCount);
			memcpy(result.m_indices.data(), indexBlob->m_data.data(), indexCount * sizeof(uint32_t));
		}
		else
		{
			LogError("Unexpected index data size");
			return false;
		}
		progCb(70);

		uint32_t meshCount = loadedFile->m_header["MeshCount"];
		auto meshBlob = loadedFile->GetBlob("Meshes");
		if ((meshCount * sizeof(ModelPart)) == meshBlob->m_data.size())
		{
			result.m_parts.resize(meshCount);
			memcpy(result.m_parts.data(), meshBlob->m_data.data(), meshCount * sizeof(ModelPart));
		}
		else
		{
			LogError("Unexpected mesh data size");
			return false;
		}
		progCb(90);

		uint32_t materialCount = loadedFile->m_header["MaterialCount"];
		auto materialBlob = loadedFile->GetBlob("Materials");
		if ((materialCount * sizeof(BakedMaterial)) == materialBlob->m_data.size())
		{
			std::vector<BakedMaterial> bakedMaterials;
			bakedMaterials.resize(materialCount);
			memcpy(bakedMaterials.data(), materialBlob->m_data.data(), materialCount * sizeof(BakedMaterial));
			result.m_materials.resize(materialCount);
			for (uint32_t i = 0; i < materialCount; ++i)
			{
				ParseBakedMaterial(bakedMaterials[i], result.m_materials[i]);
			}
		}
		else
		{
			LogError("Unexpected material data size");
			return false;
		}
		progCb(100);

		return true;
	}

	bool SaveBakedModel(std::string_view srcPath, std::string_view bakedPath, const ModelData& modelData)
	{
		R3_PROF_EVENT();

		AssetFile bakedFile;
		bakedFile.m_header["Version"] = c_bakedModelVersion;
		bakedFile.m_header["SourceFile"] = srcPath;
		bakedFile.m_header["VertexCount"] = modelData.m_vertices.size();
		bakedFile.m_header["IndexCount"] = modelData.m_indices.size();
		bakedFile.m_header["MaterialCount"] = modelData.m_materials.size();
		bakedFile.m_header["MeshCount"] = modelData.m_parts.size();
		auto& boundsMinJson = bakedFile.m_header["BoundsMin"];
		boundsMinJson["X"] = modelData.m_boundsMin.x;
		boundsMinJson["Y"] = modelData.m_boundsMin.y;
		boundsMinJson["Z"] = modelData.m_boundsMin.z;
		auto& boundsMaxJson = bakedFile.m_header["BoundsMax"];
		boundsMaxJson["X"] = modelData.m_boundsMax.x;
		boundsMaxJson["Y"] = modelData.m_boundsMax.y;
		boundsMaxJson["Z"] = modelData.m_boundsMax.z;

		// quantise vertices here
		struct QuantisedValueStats {
			float m_minValue = FLT_MAX;
			float m_maxValue = -FLT_MAX;
			double m_minError = DBL_MAX;
			double m_maxError = -DBL_MAX;
		};
		auto quantiseToF16 = [](const float v, QuantisedValueStats& stats) -> uint16_t {
			const uint16_t quantised = meshopt_quantizeHalf(v);
			const float decompressed = meshopt_dequantizeHalf(quantised);
			const double errorVal = fabs((double)decompressed - (double)v);
			stats.m_minValue = glm::min(v, stats.m_minValue);
			stats.m_maxValue = glm::max(v, stats.m_maxValue);
			stats.m_minError = glm::min(errorVal, stats.m_minError);
			stats.m_maxError = glm::max(errorVal, stats.m_maxError);
			return quantised;
		};
		auto logStats = [](std::string_view name, const QuantisedValueStats& stats)
		{
			if (stats.m_maxError > 0.001)		// 0.1% error threshold
			{
				LogInfo("Quantised {} has error > 0.001", name);
				LogInfo("\tMin Value: {}, Max Value: {}", stats.m_minValue, stats.m_maxValue);
				LogInfo("\tMin Error: {:.6f}, Max Error: {:.6f}", stats.m_minError, stats.m_maxError);
			}
		};
		std::vector<BakedModelVertexPosUV> bakedVerticesPosUV(modelData.m_vertices.size());
		std::vector<BakedModelVertexNormalTangent> bakedVerticesNormTan(modelData.m_vertices.size());
		QuantisedValueStats quantUV_x, quantUV_y;
		QuantisedValueStats quantNormals;
		QuantisedValueStats quantTangents;
		for (int v = 0; v < modelData.m_vertices.size(); ++v)
		{
			BakedModelVertexPosUV& posUV = bakedVerticesPosUV[v];
			BakedModelVertexNormalTangent& normTan = bakedVerticesNormTan[v];
			posUV.m_position[0] = modelData.m_vertices[v].m_positionU0[0];
			posUV.m_position[1] = modelData.m_vertices[v].m_positionU0[1];
			posUV.m_position[2] = modelData.m_vertices[v].m_positionU0[2];
			posUV.m_uv0[0] = quantiseToF16(modelData.m_vertices[v].m_positionU0[3], quantUV_x);
			posUV.m_uv0[1] = quantiseToF16(modelData.m_vertices[v].m_normalV0[3], quantUV_y);
			normTan.m_normal[0] = quantiseToF16(modelData.m_vertices[v].m_normalV0[0], quantNormals);
			normTan.m_normal[1] = quantiseToF16(modelData.m_vertices[v].m_normalV0[1], quantNormals);
			normTan.m_normal[2] = quantiseToF16(modelData.m_vertices[v].m_normalV0[2], quantNormals);
			normTan.m_tangent[0] = quantiseToF16(modelData.m_vertices[v].m_tangentPad[0], quantTangents);
			normTan.m_tangent[1] = quantiseToF16(modelData.m_vertices[v].m_tangentPad[1], quantTangents);
			normTan.m_tangent[2] = quantiseToF16(modelData.m_vertices[v].m_tangentPad[2], quantTangents);
		}
		logStats("UV X", quantUV_x);
		logStats("UV Y", quantUV_y);
		logStats("Normals", quantNormals);
		logStats("Tangents", quantTangents);

		AssetFile::Blob& vertexPosUVBlob = bakedFile.m_blobs.emplace_back();
		vertexPosUVBlob.m_name = "VerticesPosUV";
		vertexPosUVBlob.m_data.resize(sizeof(BakedModelVertexPosUV) * bakedVerticesPosUV.size());
		memcpy(vertexPosUVBlob.m_data.data(), bakedVerticesPosUV.data(), sizeof(BakedModelVertexPosUV) * bakedVerticesPosUV.size());

		AssetFile::Blob& vertexPosNormTan = bakedFile.m_blobs.emplace_back();
		vertexPosNormTan.m_name = "VerticesNormTan";
		vertexPosNormTan.m_data.resize(sizeof(BakedModelVertexNormalTangent)* bakedVerticesNormTan.size());
		memcpy(vertexPosNormTan.m_data.data(), bakedVerticesNormTan.data(), sizeof(BakedModelVertexNormalTangent)* bakedVerticesNormTan.size());

		AssetFile::Blob& indexBlob = bakedFile.m_blobs.emplace_back();
		indexBlob.m_name = "Indices";
		indexBlob.m_data.resize(sizeof(uint32_t) * modelData.m_indices.size());
		memcpy(indexBlob.m_data.data(), modelData.m_indices.data(), sizeof(uint32_t) * modelData.m_indices.size());

		AssetFile::Blob& meshesBlob = bakedFile.m_blobs.emplace_back();
		meshesBlob.m_name = "Meshes";
		meshesBlob.m_data.resize(sizeof(ModelPart) * modelData.m_parts.size());
		memcpy(meshesBlob.m_data.data(), modelData.m_parts.data(), sizeof(ModelPart) * modelData.m_parts.size());

		std::vector<BakedMaterial> bakedMaterials;
		bakedMaterials.resize(modelData.m_materials.size());
		for (int m = 0; m < modelData.m_materials.size(); ++m)
		{
			if (!MakeBakedMaterial(modelData.m_materials[m], bakedMaterials[m]))
			{
				return false;
			}
		}
		AssetFile::Blob& materialsBlob = bakedFile.m_blobs.emplace_back();
		materialsBlob.m_name = "Materials";
		materialsBlob.m_data.resize(sizeof(BakedMaterial) * bakedMaterials.size());
		memcpy(materialsBlob.m_data.data(), bakedMaterials.data(), sizeof(BakedMaterial) * bakedMaterials.size());

		if (!SaveAssetFile(bakedFile, bakedPath))
		{
			LogError("Failed to save baked asset {}", bakedPath);
			return false;
		}

		return true;
	}

	void OptimiseModel(ModelData& sourceModel, const BakeSettings& settings, ProgressCb progCb)
	{
		R3_PROF_EVENT();

		// run meshoptimizer on each model part, rebuild the vb/ib for the entire model
		std::vector<uint32_t> allMeshIndices;		// vb/ib for the entire model (with re-patched indices to reference one giant buffer)
		std::vector<ModelVertex> allMeshVertices;
		float progress = 50.0f;
		for (int part = 0; part < sourceModel.m_parts.size(); ++part)
		{
			std::vector<uint32_t> remapTable;
			std::vector<uint32_t> newPartIndices;		// vb/ib for each part
			std::vector<ModelVertex> newPartVertices;
			auto sourceIndexCount = sourceModel.m_parts[part].m_indexCount;
			auto sourceVerticesCount = sourceModel.m_parts[part].m_vertexCount;
			auto sourceIndicesPtr = sourceModel.m_indices.data() + sourceModel.m_parts[part].m_indexDataOffset;
			auto sourceVerticesPtr = sourceModel.m_vertices.data() + sourceModel.m_parts[part].m_vertexDataOffset;

			// Part indices are relative to the entire vertex buffer. Remap them back to the 'local' vertex buffer
			for (uint32_t i = 0; i < sourceIndexCount; ++i)
			{
				sourceModel.m_indices[i + sourceModel.m_parts[part].m_indexDataOffset] -= sourceModel.m_parts[part].m_vertexDataOffset;
				assert(sourceModel.m_indices[i + sourceModel.m_parts[part].m_indexDataOffset] < sourceVerticesCount);
			}

			// First build a remap table
			remapTable.resize(sourceVerticesCount);
			size_t newVertexCount = meshopt_generateVertexRemap(remapTable.data(), sourceIndicesPtr, sourceIndexCount, sourceVerticesPtr, sourceVerticesCount, sizeof(ModelVertex));

			// Generate a new set of indices + vertices based on remap table
			newPartIndices.resize(sourceIndexCount);
			newPartVertices.resize(newVertexCount);
			meshopt_remapIndexBuffer(newPartIndices.data(), sourceIndicesPtr, sourceIndexCount, remapTable.data());
			meshopt_remapVertexBuffer(newPartVertices.data(), sourceVerticesPtr, sourceVerticesCount, sizeof(ModelVertex), remapTable.data());

			// Optimise indices for vertex cache
			meshopt_optimizeVertexCache(newPartIndices.data(), newPartIndices.data(), newPartIndices.size(), newPartVertices.size());

			// Optimise indices for overdraw, sacrificing some amount of cache efficiency
			meshopt_optimizeOverdraw(newPartIndices.data(), newPartIndices.data(), newPartIndices.size(), (const float*)newPartVertices.data(), newPartVertices.size(), sizeof(ModelVertex), settings.m_optimiseOverdrawThreshold);

			// Re-order the vertices and indices for optimal vertex fetch
			meshopt_optimizeVertexFetch(newPartVertices.data(), newPartIndices.data(), newPartIndices.size(), newPartVertices.data(), newPartVertices.size(), sizeof(ModelVertex));

			if (settings.m_simplifyModel)
			{
				std::vector<uint32_t> simplifiedIndices(newPartIndices.size());	// reserve enough memory for new indices
				uint32_t targetIndexCount = (uint32_t)((float)newPartIndices.size() * settings.m_simplifyIndexThreshold);
				float resultError = 0.0f;

				const float attribWeights[] = { 1.0f,1.0f,1.0f,1.0f,1.0f };
				size_t newIndexCount = meshopt_simplifyWithAttributes(simplifiedIndices.data(),
					newPartIndices.data(),
					newPartIndices.size(),
					(const float*)newPartVertices.data(),
					newPartVertices.size(),
					sizeof(ModelVertex),
					&newPartVertices[0].m_positionU0[3],	// start at uv0
					sizeof(ModelVertex),
					attribWeights,
					5,										// uv0 + normal + uv1
					nullptr,
					targetIndexCount,
					settings.m_simplifyTargetError,
					0,
					&resultError
				);

				LogInfo("Simplified mesh from {} to {} triangles, final error = {} (target error = {})", newPartIndices.size() / 2, targetIndexCount / 3,resultError, settings.m_simplifyTargetError);

				newPartIndices.clear();
				newPartIndices.insert(newPartIndices.end(), simplifiedIndices.begin(), simplifiedIndices.begin() + newIndexCount);

				// re-optimise indices
				meshopt_optimizeVertexCache(newPartIndices.data(), newPartIndices.data(), newPartIndices.size(), newPartVertices.size());
				meshopt_optimizeOverdraw(newPartIndices.data(), newPartIndices.data(), newPartIndices.size(), (const float*)newPartVertices.data(), newPartVertices.size(), sizeof(ModelVertex), settings.m_optimiseOverdrawThreshold);
				meshopt_optimizeVertexFetch(newPartVertices.data(), newPartIndices.data(), newPartIndices.size(), newPartVertices.data(), newPartVertices.size(), sizeof(ModelVertex));
			}

			// Patch the part data with the new vb/ib, append the data to the new mesh vb/ib
			auto newMeshVbOffset = static_cast<uint32_t>(allMeshVertices.size());
			auto newMeshIbOffset = static_cast<uint32_t>(allMeshIndices.size());
			for (auto i = 0; i < newPartIndices.size(); ++i)
			{
				newPartIndices[i] += newMeshVbOffset;	// offset indices into final vertex buffer
			}
			allMeshVertices.insert(allMeshVertices.end(), newPartVertices.begin(), newPartVertices.end());
			allMeshIndices.insert(allMeshIndices.end(), newPartIndices.begin(), newPartIndices.end());

			sourceModel.m_parts[part].m_indexDataOffset = newMeshIbOffset;
			sourceModel.m_parts[part].m_vertexDataOffset = newMeshVbOffset;
			sourceModel.m_parts[part].m_indexCount = (uint32_t)newPartIndices.size();
			sourceModel.m_parts[part].m_vertexCount = (uint32_t)newVertexCount;

			progress += 45.0f / (float)sourceModel.m_parts.size();		// up to 95% progress
			progCb((int)progress);
		}

		// copy the final vb/ib for the entire model
		sourceModel.m_vertices.clear();
		sourceModel.m_vertices.insert(sourceModel.m_vertices.end(), allMeshVertices.begin(), allMeshVertices.end());
		sourceModel.m_indices.clear();
		sourceModel.m_indices.insert(sourceModel.m_indices.end(), allMeshIndices.begin(), allMeshIndices.end());
	}

	bool BakeModel(std::string_view filePath, ProgressCb progCb)
	{
		R3_PROF_EVENT();
		std::string bakedPath = GetBakedModelPath(filePath);
		if (bakedPath.empty())
		{
			LogInfo("Invalid path for model baking - {}", filePath);
			return false;
		}
		if (std::filesystem::exists(bakedPath))
		{
			return true;	// already baked!
		}

		BakeSettings modelBakeSettings;
		LoadBakeSettings(filePath, modelBakeSettings);

		auto loadProgressCb = [&](int p)	// first 50% of progress = loading the file
		{
			progCb(p / 2);
		};

		AssimpLoadSettings assimpSettings;	// fill in assimp settings from bake options
		assimpSettings.m_preTransformVertices = modelBakeSettings.m_flattenMeshes;
		assimpSettings.m_fixInFacingNormals = modelBakeSettings.m_fixInFacingNormals;
		assimpSettings.m_improveCacheLocality = modelBakeSettings.m_improveCacheLocality;
		assimpSettings.m_splitLargeMeshes = modelBakeSettings.m_splitLargeMeshes;

		ModelData sourceModel;
		if (!LoadModelDataAssimp(filePath, sourceModel, assimpSettings, loadProgressCb))
		{
			LogError("Failed to load source model {}", filePath);
			return false;
		}

		if (modelBakeSettings.m_runMeshOptimizer)
		{
			OptimiseModel(sourceModel, modelBakeSettings, progCb);
		}

		if (!SaveBakedModel(filePath, bakedPath, sourceModel))
		{
			LogError("Failed to save baked model {} to file {}", filePath, bakedPath);
			return false;
		}
		progCb(100);

		return true;
	}

	bool LoadModelData(std::string_view filePath, ModelData& result, ProgressCb progCb)
	{
		char debugName[1024] = { '\0' };
		sprintf_s(debugName, "LoadModelData %s", filePath.data());
		R3_PROF_EVENT_DYN(debugName);

		auto fileExtension = std::filesystem::path(filePath).extension().string();
		if (fileExtension == c_bakedModelExtension + std::to_string(c_bakedModelVersion))
		{
			return LoadBakedModel(filePath, result, progCb);
		}
		else
		{
			AssimpLoadSettings assimpSettings;
			return LoadModelDataAssimp(filePath, result, assimpSettings, progCb);
		}
	}
}