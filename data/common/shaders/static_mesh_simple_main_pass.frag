#version 450
#extension GL_EXT_buffer_reference : require

#include "mesh_data.h"
#include "static_mesh_simple_shared.h"
#include "pbr.h"
#include "utils.h"

layout(location = 0) in vec3 inWorldSpacePos;
layout(location = 1) in vec3 inWorldspaceNormal;
layout(location = 0) out vec4 outColour;

void main() {
	GlobalConstants globals = PushConstants.m_globals.m_allGlobals[PushConstants.m_globalIndex];

	vec3 worldPos = inWorldSpacePos;
	vec3 normal = normalize(inWorldspaceNormal);
	vec3 viewDir = normalize(globals.m_cameraWorldSpacePos.xyz - worldPos);
	
	int materialIndex = PushConstants.m_materialIndex;
	StaticMeshMaterial myMaterial = globals.m_materialBuffer.materials[materialIndex];
	
	vec3 lightPos = vec3(20, 30, 15);
	vec3 lightColour = vec3(1000,1000,1000);
	
	PBRMaterial mat;
	mat.m_albedo = vec4(myMaterial.m_albedoOpacity.xyz,1).xyz;
	mat.m_metallic = myMaterial.m_metallic;
	mat.m_roughness = myMaterial.m_roughness;
	mat.m_ao = 0.75;
	mat.m_ambientMulti = 0.02;
	
	vec3 directLight = PBRDirectLighting(viewDir, worldPos, normal, lightPos, lightColour, mat);
	
	// tonemap using reinhard operator for now 
    directLight = directLight / (directLight + vec3(1.0));
	
	outColour = vec4(LinearToSRGB(directLight),myMaterial.m_albedoOpacity.a);
}