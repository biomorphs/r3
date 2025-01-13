#version 450
#extension GL_EXT_buffer_reference : require

#include "static_mesh_simple_shared.h"
#include "pbr.h"
#include "utils.h"

layout(location = 0) in vec3 inWorldSpacePos;
layout(location = 1) in vec3 inWorldspaceNormal;
layout(location = 2) in vec2 inUV;
layout(location = 3) in flat uint inMaterialIndex;
layout(location = 4) in mat3 inTBN;
layout(location = 0) out vec4 outColour;

// Applies normal map if the material has one
vec3 GetWorldspaceNormal(StaticMeshMaterial material, vec2 texCoords)
{
	vec3 normal = normalize(inWorldspaceNormal);
	if(material.m_normalTexture != -1)	// normal mapping
	{
		normal = texture(allTextures[material.m_normalTexture],texCoords).xyz;
		normal = normalize(normal * 2.0 - 1.0);
		normal = normalize(inTBN * normal);
	}
	return normal;
}

void main() {
	GlobalConstants globals = PushConstants.m_globals.AllGlobals[0];
	vec3 viewDir = normalize(globals.m_cameraWorldSpacePos.xyz - inWorldSpacePos);
	StaticMeshMaterial myMaterial = globals.m_materialBuffer.materials[inMaterialIndex];
	vec3 normal = GetWorldspaceNormal(myMaterial, inUV);
	
	PBRMaterial mat;
	float finalAlpha = myMaterial.m_albedoOpacity.a;
	if(myMaterial.m_albedoTexture != -1)
	{
		vec4 albedoTex = texture(allTextures[myMaterial.m_albedoTexture],inUV);
		mat.m_albedo = myMaterial.m_albedoOpacity.xyz * SRGBToLinear(albedoTex).xyz;
		finalAlpha = finalAlpha * albedoTex.a;
	}
	else
	{
		mat.m_albedo = myMaterial.m_albedoOpacity.xyz;
	}
	
	// if(finalAlpha < 0.25)	// punch-through alpha, may want a material param for this?
	// {
	// 	discard;
	// }
	
	mat.m_roughness = (myMaterial.m_roughnessTexture!=-1) ? texture(allTextures[myMaterial.m_roughnessTexture],inUV).r : myMaterial.m_roughness;
	mat.m_metallic = (myMaterial.m_metalnessTexture != -1) ? texture(allTextures[myMaterial.m_metalnessTexture],inUV).r : myMaterial.m_metallic;
	mat.m_ao = (myMaterial.m_aoTexture != -1) ? texture(allTextures[myMaterial.m_aoTexture],inUV).x : 1.0;
	
	AllLightsData lightsData = globals.m_lightsBuffer.data[0];
	
	// Apply sun direct light
	vec4 sunDirectionBrightness = lightsData.m_sunDirectionBrightness;
	vec3 sunRadiance = lightsData.m_sunColourAmbient.xyz * sunDirectionBrightness.w;
	vec3 directLight = PBRDirectLighting(mat, viewDir, -sunDirectionBrightness.xyz, normal, sunRadiance, 1.0);
	
	// Apply point lights (direct)
	uint pointLightCount = lightsData.m_pointlightCount;
	for(uint p=0;p<pointLightCount;++p)
	{
		Pointlight pl = lightsData.m_allPointlights.lights[p];
		vec3 lightRadiance = pl.m_colourBrightness.xyz * pl.m_colourBrightness.w;
		float attenuation = GetPointlightAttenuation(pl, inWorldSpacePos);
		vec3 lightToPixel = normalize(pl.m_positionDistance.xyz - inWorldSpacePos);
		directLight += PBRDirectLighting(mat, viewDir, lightToPixel, normal, lightRadiance, attenuation);
	}
	
	// Ambient is a hack, tries to combine sky + sun colour somehow
	vec3 ambient = PBRGetAmbientLighting(mat, 
		lightsData.m_sunColourAmbient.xyz, 
		lightsData.m_sunColourAmbient.w,
		lightsData.m_skyColourAmbient.xyz,
		lightsData.m_skyColourAmbient.w
	);
	
	vec3 finalLight = ambient + directLight;
	outColour = vec4(finalLight,finalAlpha);
}