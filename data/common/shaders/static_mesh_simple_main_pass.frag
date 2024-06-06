#version 450
#extension GL_EXT_buffer_reference : require
//#extension GL_EXT_nonuniform_qualifier : enable

#include "static_mesh_simple_shared.h"
#include "pbr.h"
#include "utils.h"

layout(location = 0) in vec3 inWorldSpacePos;
layout(location = 1) in vec3 inWorldspaceNormal;
layout(location = 2) in vec2 inUV;
layout(location = 0) out vec4 outColour;

void main() {
	GlobalConstants globals = PushConstants.m_globals.m_allGlobals[PushConstants.m_globalIndex];
	vec3 worldPos = inWorldSpacePos;
	vec3 normal = normalize(inWorldspaceNormal);
	vec3 viewDir = normalize(globals.m_cameraWorldSpacePos.xyz - worldPos);
	int materialIndex = PushConstants.m_materialIndex;
	
	StaticMeshMaterial myMaterial = globals.m_materialBuffer.materials[materialIndex];
	PBRMaterial mat;
	if(myMaterial.m_albedoTexture != -1)
	{
		mat.m_albedo = myMaterial.m_albedoOpacity.xyz * SRGBToLinear(texture(allTextures[myMaterial.m_albedoTexture],inUV)).xyz;
	}
	else
	{
		mat.m_albedo = myMaterial.m_albedoOpacity.xyz;
	}
	mat.m_metallic = myMaterial.m_metallic;
	mat.m_roughness = myMaterial.m_roughness;
	mat.m_ao = 1.0;
	mat.m_ambientMulti = 0.00001;
	
	uint pointLightCount = globals.m_pointLightCount;
	vec3 directLight = vec3(0.0,0.0,0.0);
	for(uint p=0;p<pointLightCount;++p)
	{
		Pointlight pl = globals.m_pointlightBuffer.lights[globals.m_firstPointLightOffset + p];
		vec3 lightRadiance = pl.m_colourBrightness.xyz * pl.m_colourBrightness.w;
		float attenuation = GetPointlightAttenuation(pl, worldPos);
		directLight += PBRDirectLighting(mat, viewDir, worldPos, normal, pl.m_positionDistance.xyz, lightRadiance, attenuation);
	}
	
	vec3 finalLight = PBRGetAmbientLighting(mat) + directLight;	
	
	// tonemap using reinhard operator for now 
	// should be a separate fullscreen pass
    finalLight = finalLight / (finalLight + vec3(1.0));
	
#ifdef R3_CONVERT_OUTPUT_TO_SRGB
	finalLight = LinearToSRGB(finalLight);
#endif

	outColour = vec4(finalLight,myMaterial.m_albedoOpacity.a);
}