#version 450
#extension GL_EXT_buffer_reference : require
//#extension GL_EXT_nonuniform_qualifier : enable

#include "static_mesh_simple_shared.h"
#include "pbr.h"
#include "utils.h"

layout(location = 0) in vec3 inWorldSpacePos;
layout(location = 1) in vec3 inWorldspaceNormal;
layout(location = 2) in vec2 inUV;
layout(location = 3) in mat3 inTBN;
layout(location = 0) out vec4 outColour;

void main() {
	GlobalConstants globals = PushConstants.m_globals.m_allGlobals[PushConstants.m_globalIndex];
	vec3 worldPos = inWorldSpacePos;
	vec3 normal = normalize(inWorldspaceNormal);
	vec3 viewDir = normalize(globals.m_cameraWorldSpacePos.xyz - worldPos);
	int materialIndex = PushConstants.m_materialIndex;
	StaticMeshMaterial myMaterial = globals.m_materialBuffer.materials[materialIndex];
	
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
	
	// early discard for punch-through alpha 
	if(finalAlpha < 0.25)
	{
		discard;
	}
	
	if(myMaterial.m_roughnessTexture != -1)
	{
		mat.m_roughness = texture(allTextures[myMaterial.m_roughnessTexture],inUV).y;	// assuming combined rough/metalness
	}
	else
	{
		mat.m_roughness = myMaterial.m_roughness;
	}
	if(myMaterial.m_metalnessTexture != -1)
	{
		mat.m_metallic = texture(allTextures[myMaterial.m_metalnessTexture],inUV).z;	// assuming combined rough/metalness
	}
	else
	{
		mat.m_metallic = myMaterial.m_metallic;
	}
	if(myMaterial.m_aoTexture != -1)
	{
		mat.m_ao = texture(allTextures[myMaterial.m_aoTexture],inUV).x;
	}
	else
	{
		mat.m_ao = 1.0;
	}
	
	if(myMaterial.m_normalTexture != -1)	// normal mapping
	{
		normal = texture(allTextures[myMaterial.m_normalTexture],inUV).xyz;
		normal = normalize(normal * 2.0 - 1.0);
		normal = normalize(inTBN * normal);
	}
	
	// Apply sun direct light
	vec3 sunRadiance = globals.m_sunColourAmbient.xyz * globals.m_sunDirectionBrightness.w;
	vec3 directLight = PBRDirectLighting(mat, viewDir, -globals.m_sunDirectionBrightness.xyz, normal, sunRadiance, 1.0);
	
	// Apply point lights (direct)
	uint pointLightCount = globals.m_pointLightCount;
	for(uint p=0;p<pointLightCount;++p)
	{
		Pointlight pl = globals.m_pointlightBuffer.lights[globals.m_firstPointLightOffset + p];
		vec3 lightRadiance = pl.m_colourBrightness.xyz * pl.m_colourBrightness.w;
		float attenuation = GetPointlightAttenuation(pl, worldPos);
		vec3 lightToPixel = normalize(pl.m_positionDistance.xyz - worldPos);
		directLight += PBRDirectLighting(mat, viewDir, lightToPixel, normal, lightRadiance, attenuation);
	}
	
	// Ambient is a hack, tries to combine sky + sun colour somehow
	vec3 ambient = PBRGetAmbientLighting(mat, 
		globals.m_sunColourAmbient.xyz, 
		globals.m_sunColourAmbient.w,
		globals.m_skyColourAmbient.xyz,
		globals.m_skyColourAmbient.w
	);
	
	vec3 finalLight = ambient + directLight;	
	
	// tonemap using reinhard operator for now 
	// should be a separate fullscreen pass
    finalLight = finalLight / (finalLight + vec3(1.0));
	
#ifdef R3_CONVERT_OUTPUT_TO_SRGB
	finalLight = LinearToSRGB(finalLight);
#endif

	outColour = vec4(finalLight,finalAlpha);
}