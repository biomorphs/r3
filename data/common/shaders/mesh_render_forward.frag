#version 450
#extension GL_EXT_buffer_reference : require

#include "mesh_render_shared.h"
#include "pbr.h"
#include "utils.h"
#include "sun_shadows.h"

layout(location = 0) in vec3 inWorldSpacePos;
layout(location = 1) in vec3 inWorldspaceNormal;
layout(location = 2) in vec2 inUV;
layout(location = 3) in flat uint inInstanceIndex;
layout(location = 4) in mat3 inTBN;
layout(location = 0) out vec4 outColour;

void main() {
	Globals globals = PushConstants.m_globals.data[0];
	vec3 viewDir = normalize(globals.m_cameraWorldSpacePos.xyz - inWorldSpacePos);
	MeshInstanceData thisInstance = PushConstants.m_instances.data[inInstanceIndex];
	MeshMaterial myMaterial = thisInstance.m_material.data[0];
	vec3 normal = GetWorldspaceNormal(inWorldspaceNormal, myMaterial.m_normalTexture, inTBN, inUV);
	
	PBRMaterial mat;
	float finalAlpha = myMaterial.m_albedoOpacity.a;
	if(myMaterial.m_albedoTexture != -1)
	{
		vec4 albedoTex = texture(AllTextures[myMaterial.m_albedoTexture],inUV);
		mat.m_albedo = myMaterial.m_albedoOpacity.xyz * SRGBToLinear(albedoTex).xyz;
		finalAlpha = finalAlpha * albedoTex.a;
	}
	else
	{
		mat.m_albedo = myMaterial.m_albedoOpacity.xyz;
	}
	
	if((myMaterial.m_flags & MESH_MATERIAL_ALPHA_PUNCH_FLAG) != 0 && finalAlpha < 0.5)
	{
		discard;
	}
	
	mat.m_roughness = (myMaterial.m_roughnessTexture!=-1) ? texture(AllTextures[myMaterial.m_roughnessTexture],inUV).r : myMaterial.m_roughness;
	mat.m_metallic = (myMaterial.m_metalnessTexture != -1) ? texture(AllTextures[myMaterial.m_metalnessTexture],inUV).r : myMaterial.m_metallic;
	mat.m_ao = (myMaterial.m_aoTexture != -1) ? texture(AllTextures[myMaterial.m_aoTexture],inUV).x : 1.0;
	
	LightsData lightsData = globals.m_lightsBuffer.data[0];
	
	// Apply sun direct light
	float sunShadow = CalculateSunShadow(lightsData.m_shadows, globals.m_worldToViewTransform, inWorldSpacePos);
	vec4 sunDirectionBrightness = lightsData.m_sunDirectionBrightness;
	vec3 sunRadiance = sunShadow * lightsData.m_sunColourAmbient.xyz * sunDirectionBrightness.w;
	vec3 directLight = PBRDirectLighting(mat, viewDir, -sunDirectionBrightness.xyz, normal, sunRadiance, 1.0);
	
	// Apply point lights (direct)
#ifdef USE_TILED_LIGHTS
	LightTileMetadata tileMetadata = globals.m_lightTileMetadata.data[0];
	vec2 thisFragment = gl_FragCoord.xy;	// always returns pixel center (i.e. offset by 0.5)
	uint tileIndex = GetLightTileIndex(uvec2(floor(thisFragment)), tileMetadata.m_tileCount);
	LightTile thisLightTile = tileMetadata.m_lightTiles.data[tileIndex];
	
	// tile light count also takes into account global light count (since the light tile data will not be valid when there are 0 lights!)
	uint pointLightCount = min(lightsData.m_pointlightCount, thisLightTile.m_lightIndexCount);	
	uint firstLightOffset = thisLightTile.m_firstLightIndex;
	uint lastLightOffset = firstLightOffset + pointLightCount;
	for(uint l=firstLightOffset;l<lastLightOffset;++l)
	{
		uint p = tileMetadata.m_lightIndices.data[l];
#else
	uint pointLightCount = lightsData.m_pointlightCount;
	for(uint p=0;p<pointLightCount;++p)
	{
#endif
		Pointlight pl = lightsData.m_allPointlights.data[p];
		vec3 lightRadiance = pl.m_colourBrightness.xyz * pl.m_colourBrightness.w;
		vec3 pixelToLight = pl.m_positionDistance.xyz - inWorldSpacePos;
		float attenuation = GetPointlightAttenuation(pl, pixelToLight);
		directLight += PBRDirectLighting(mat, viewDir, normalize(pixelToLight), normal, lightRadiance, attenuation);
	}
	
	// Apply spot lights (direct)
	uint spotLightCount = lightsData.m_spotlightCount;
	for(uint s=0;s<spotLightCount;++s)
	{
		Spotlight sl = lightsData.m_allSpotlights.data[s];
		vec3 pixelToLight = sl.m_positionDistance.xyz - inWorldSpacePos;
		vec3 pixelToLightDir = normalize(pixelToLight);
		float attenuation = GetSpotlightAttenuation(sl, pixelToLight, pixelToLightDir);
		directLight += PBRDirectLighting(mat, viewDir, pixelToLightDir, normal, sl.m_colourOuterAngle.xyz, attenuation);
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