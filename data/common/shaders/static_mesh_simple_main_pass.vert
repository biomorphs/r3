#version 450
#extension GL_EXT_buffer_reference : require

#include "mesh_data.h"

// transform and vertex buffer set in push constants
layout(push_constant) uniform constants
{
	mat4 m_instanceTransform;
	mat4 m_projViewTransform;
	vec4 m_cameraWorldPosition;
	VertexBuffer vertexBuffer;
} PushConstants;

layout(location = 0) out vec3 outWorldspacePos;
layout(location = 1) out vec3 outWorldspaceNormal;

void main() {
	MeshVertex v = PushConstants.vertexBuffer.vertices[gl_VertexIndex];
	vec4 worldSpacePosition = PushConstants.m_instanceTransform * vec4(v.m_positionU0.xyz, 1);
	outWorldspacePos = worldSpacePosition.xyz;
	outWorldspaceNormal = normalize(mat3(transpose(inverse(PushConstants.m_instanceTransform))) * v.m_normalV0.xyz);
	gl_Position = PushConstants.m_projViewTransform * worldSpacePosition;	// clip space
}