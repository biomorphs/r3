#version 450
#extension GL_EXT_buffer_reference : require

#include "mesh_data.h"
#include "pbr.h"
#include "utils.h"

// transform and vertex buffer set in push constants
layout(push_constant) uniform constants
{
	mat4 m_instanceTransform;
	mat4 m_projViewTransform;
	vec4 m_cameraWorldPosition;
	VertexBuffer vertexBuffer;
} PushConstants;

layout(location = 0) in vec3 inWorldSpacePos;
layout(location = 1) in vec3 inWorldspaceNormal;
layout(location = 0) out vec4 outColour;

void main() {
	vec3 worldPos = inWorldSpacePos;
	vec3 normal = normalize(inWorldspaceNormal);
	vec3 viewDir = normalize(PushConstants.m_cameraWorldPosition.xyz - worldPos);
	
	vec3 lightPos = vec3(15, 10, 12);
	vec3 lightColour = vec3(1000,1000,1000);
	
	PBRMaterial mat;
	mat.m_albedo = vec3(1.00, 0.71, 0.29);
	mat.m_metallic = 0.0;
	mat.m_roughness = 0.25;
	mat.m_ao = 1.0;
	mat.m_ambientMulti = 0.03;
	
	vec3 directLight = PBRDirectLighting(viewDir, worldPos, normal, lightPos, lightColour, mat);
	outColour = vec4(LinearToSRGB(directLight),1);
}