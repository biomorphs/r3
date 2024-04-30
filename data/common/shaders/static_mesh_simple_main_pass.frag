#version 450
#extension GL_EXT_buffer_reference : require

#include "mesh_data.h"
#include "pbr.h"
#include "utils.h"

layout(buffer_reference, std430) readonly buffer GlobalConstants { 
	mat4 m_projViewTransform;
	vec4 m_cameraWorldSpacePos;
	VertexBuffer m_vertexBuffer;
};

// transform and globals buffer addr sent per mesh
layout(push_constant) uniform constants
{
	mat4 m_instanceTransform;
	GlobalConstants m_globalConstants;
	int m_globalIndex;
} PushConstants;

layout(location = 0) in vec3 inWorldSpacePos;
layout(location = 1) in vec3 inWorldspaceNormal;
layout(location = 0) out vec4 outColour;

void main() {
	vec3 worldPos = inWorldSpacePos;
	vec3 normal = normalize(inWorldspaceNormal);
	vec3 viewDir = normalize(PushConstants.m_globalConstants.m_cameraWorldSpacePos.xyz - worldPos);
	
	vec3 lightPos = vec3(30, 30, 30);
	vec3 lightColour = vec3(3000,3000,3000);
	
	PBRMaterial mat;
	mat.m_albedo = vec3(1.00, 0.71, 0.29);
	mat.m_metallic = 0.0;
	mat.m_roughness = 0.8;
	mat.m_ao = 1.0;
	mat.m_ambientMulti = 0.03;
	
	vec3 directLight = PBRDirectLighting(viewDir, worldPos, normal, lightPos, lightColour, mat);
	outColour = vec4(LinearToSRGB(directLight),1);
}