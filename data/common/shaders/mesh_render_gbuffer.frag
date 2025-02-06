#version 450
#extension GL_EXT_buffer_reference : require

#include "mesh_render_shared.h"
#include "utils.h"

layout(location = 0) in vec3 inWorldSpacePos;
layout(location = 1) in vec3 inWorldspaceNormal;
layout(location = 2) in vec2 inUV;
layout(location = 3) in flat uint inInstanceIndex;
layout(location = 4) in mat3 inTBN;

layout(location = 0) out vec4 outWorldSpacePositionMetal;	// world space position + metal
layout(location = 1) out vec4 outWorldNormalRoughness;	// world space normal + roughness
layout(location = 2) out vec4 outAlbedoAO;	// albedo + AO

void main() {
	MeshInstanceData thisInstance = PushConstants.m_instances.data[inInstanceIndex];
	MeshMaterial myMaterial = thisInstance.m_material.data[0];
	vec3 normal = GetWorldspaceNormal(inWorldspaceNormal, myMaterial.m_normalTexture, inTBN, inUV);
	
	vec3 albedo = myMaterial.m_albedoOpacity.xyz;
	float finalAlpha = myMaterial.m_albedoOpacity.a;
	if(myMaterial.m_albedoTexture != -1)
	{
		vec4 albedoTex = texture(AllTextures[myMaterial.m_albedoTexture],inUV);
		albedo = albedo * SRGBToLinear(albedoTex).xyz;
		finalAlpha = finalAlpha * albedoTex.a;
	}
	
	if((myMaterial.m_flags & MESH_MATERIAL_ALPHA_PUNCH_FLAG) != 0 && finalAlpha < 0.5)
	{
		discard;
	}
	
	float roughness = (myMaterial.m_roughnessTexture!=-1) ? texture(AllTextures[myMaterial.m_roughnessTexture],inUV).r : myMaterial.m_roughness;
	float metallic = (myMaterial.m_metalnessTexture != -1) ? texture(AllTextures[myMaterial.m_metalnessTexture],inUV).r : myMaterial.m_metallic;
	float ao = (myMaterial.m_aoTexture != -1) ? texture(AllTextures[myMaterial.m_aoTexture],inUV).x : 1.0;
	
	outWorldSpacePositionMetal = vec4(inWorldSpacePos,metallic);
	outWorldNormalRoughness = vec4(normal,roughness);
	outAlbedoAO = vec4(albedo,ao);
}