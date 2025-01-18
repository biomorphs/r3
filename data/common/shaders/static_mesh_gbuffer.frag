#version 450
#extension GL_EXT_buffer_reference : require

#include "static_mesh_shared.h"
#include "utils.h"

layout(location = 0) in vec3 inWorldSpacePos;
layout(location = 1) in vec3 inWorldspaceNormal;
layout(location = 2) in vec2 inUV;
layout(location = 3) in flat uint inMaterialIndex;
layout(location = 4) in mat3 inTBN;

layout(location = 0) out vec4 outWorldSpacePositionMetal;	// world space position + metal
layout(location = 1) out vec4 outWorldNormalRoughness;	// world space normal + roughness
layout(location = 2) out vec4 outAlbedoAO;	// albedo + AO

void main() {
	GlobalConstants globals = PushConstants.m_globals.AllGlobals[0];
	StaticMeshMaterial myMaterial = globals.m_materialBuffer.materials[inMaterialIndex];
	vec3 normal = GetWorldspaceNormal(inWorldspaceNormal, myMaterial.m_normalTexture, inTBN, inUV);
	
	vec3 albedo = myMaterial.m_albedoOpacity.xyz;
	float finalAlpha = myMaterial.m_albedoOpacity.a;
	if(myMaterial.m_albedoTexture != -1)
	{
		vec4 albedoTex = texture(allTextures[myMaterial.m_albedoTexture],inUV);
		albedo = albedo * SRGBToLinear(albedoTex).xyz;
		finalAlpha = finalAlpha * albedoTex.a;
	}
	
	float roughness = (myMaterial.m_roughnessTexture!=-1) ? texture(allTextures[myMaterial.m_roughnessTexture],inUV).r : myMaterial.m_roughness;
	float metallic = (myMaterial.m_metalnessTexture != -1) ? texture(allTextures[myMaterial.m_metalnessTexture],inUV).r : myMaterial.m_metallic;
	float ao = (myMaterial.m_aoTexture != -1) ? texture(allTextures[myMaterial.m_aoTexture],inUV).x : 1.0;
	
	outWorldSpacePositionMetal = vec4(inWorldSpacePos,metallic);
	outWorldNormalRoughness = vec4(normal,roughness);
	outAlbedoAO = vec4(albedo,ao);
}