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

void main() {
	GlobalConstants globals = AllGlobals[PushConstants.m_globalIndex];
	vec3 worldPos = inWorldSpacePos;
	vec3 normal = normalize(inWorldspaceNormal);
	vec3 viewDir = normalize(globals.m_cameraWorldSpacePos.xyz - worldPos);
	StaticMeshMaterial myMaterial = globals.m_materialBuffer.materials[inMaterialIndex];
	vec2 texCoords = inUV;	// may be modified by paralax mapping
	vec3 fragPosTbn;		// only set is paralax enabled
	bool paralaxEnabled = myMaterial.m_heightmapTexture != -1 && myMaterial.m_paralaxAmount > 0.0;
	if(paralaxEnabled)
	{
		vec3 viewPosTbn = inTBN * globals.m_cameraWorldSpacePos.xyz;
		fragPosTbn = inTBN * inWorldSpacePos;	// may be used later 
		texCoords = ParallaxOcclusionMapping(myMaterial.m_heightmapTexture, inUV, normalize(viewPosTbn - fragPosTbn), myMaterial.m_paralaxAmount);
		if(texCoords.x > myMaterial.m_uvOffsetScale.z || texCoords.y > myMaterial.m_uvOffsetScale.w || texCoords.x < 0.0 || texCoords.y < 0.0)
		{
			discard;
		}
	}
	PBRMaterial mat;
	float finalAlpha = myMaterial.m_albedoOpacity.a;
	if(myMaterial.m_albedoTexture != -1)
	{
		vec4 albedoTex = texture(allTextures[myMaterial.m_albedoTexture],texCoords);
		mat.m_albedo = myMaterial.m_albedoOpacity.xyz * SRGBToLinear(albedoTex).xyz;
		finalAlpha = finalAlpha * albedoTex.a;
	}
	else
	{
		mat.m_albedo = myMaterial.m_albedoOpacity.xyz;
	}
	if(finalAlpha < 0.25)	// punchthrough alpha, may want a material param for this?
	{
		discard;
	}
	mat.m_roughness = (myMaterial.m_roughnessTexture!=-1) ? texture(allTextures[myMaterial.m_roughnessTexture],texCoords).r : myMaterial.m_roughness;
	mat.m_metallic = (myMaterial.m_metalnessTexture != -1) ? texture(allTextures[myMaterial.m_metalnessTexture],texCoords).r : myMaterial.m_metallic;
	mat.m_ao = (myMaterial.m_aoTexture != -1) ? texture(allTextures[myMaterial.m_aoTexture],texCoords).x : 1.0;
	if(myMaterial.m_normalTexture != -1)	// normal mapping
	{
		normal = texture(allTextures[myMaterial.m_normalTexture],texCoords).xyz;
		normal = normalize(normal * 2.0 - 1.0);
		normal = normalize(inTBN * normal);
	}
	
	// Apply sun direct light
	vec3 sunRadiance = globals.m_sunColourAmbient.xyz * globals.m_sunDirectionBrightness.w;
	float sunShadowMul = 1.0;
	if(paralaxEnabled && myMaterial.m_paralaxShadowsEnabled > 0)
	{
		vec3 lightToPixelTangentSpace = normalize(inTBN * globals.m_sunDirectionBrightness.xyz);	// may need negating
		float initialHeight = 1.0 - texture(allTextures[myMaterial.m_heightmapTexture],texCoords).r;
		sunShadowMul = ParallaxSoftShadowMultiplier(myMaterial.m_heightmapTexture, lightToPixelTangentSpace, inUV, initialHeight, myMaterial.m_paralaxAmount);
	}
	vec3 directLight = sunShadowMul * PBRDirectLighting(mat, viewDir, -globals.m_sunDirectionBrightness.xyz, normal, sunRadiance, 1.0);
	
	// Apply point lights (direct)
	uint pointLightCount = globals.m_pointLightCount;
	for(uint p=0;p<pointLightCount;++p)
	{
		Pointlight pl = globals.m_pointlightBuffer.lights[globals.m_firstPointLightOffset + p];
		vec3 lightRadiance = pl.m_colourBrightness.xyz * pl.m_colourBrightness.w;
		float attenuation = GetPointlightAttenuation(pl, worldPos);
		vec3 lightToPixel = normalize(pl.m_positionDistance.xyz - worldPos);
		
		float shadowMul = 1.0;
		if(paralaxEnabled && myMaterial.m_paralaxShadowsEnabled > 0)
		{
			vec3 lightToPixelTangentSpace = normalize((inTBN * pl.m_positionDistance.xyz) - fragPosTbn);
			float initialHeight = 1.0 - texture(allTextures[myMaterial.m_heightmapTexture],texCoords).r;
			shadowMul = ParallaxSoftShadowMultiplier(myMaterial.m_heightmapTexture, lightToPixelTangentSpace, inUV, initialHeight, myMaterial.m_paralaxAmount);
		}
		directLight += shadowMul * PBRDirectLighting(mat, viewDir, lightToPixel, normal, lightRadiance, attenuation);
	}
	
	// Ambient is a hack, tries to combine sky + sun colour somehow
	vec3 ambient = PBRGetAmbientLighting(mat, 
		globals.m_sunColourAmbient.xyz, 
		globals.m_sunColourAmbient.w,
		globals.m_skyColourAmbient.xyz,
		globals.m_skyColourAmbient.w
	);
	
	vec3 finalLight = ambient + directLight;
	outColour = vec4(finalLight,finalAlpha);
}