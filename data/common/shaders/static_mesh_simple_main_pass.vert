#version 450
#extension GL_EXT_buffer_reference : require

#include "mesh_data.h"

struct GlobalConstants { 
	mat4 m_projViewTransform;
	vec4 m_cameraWorldSpacePos;
	VertexBuffer m_vertexBuffer;
};

// global constants stored in an array to allow overlapping frames
layout(buffer_reference, std430) readonly buffer GlobalConstantsBuffer { 
	GlobalConstants m_allGlobals[];
};

// transform and globals buffer addr/index sent per mesh
layout(push_constant) uniform constants
{
	mat4 m_instanceTransform;
	GlobalConstantsBuffer m_globals;
	int m_globalIndex;
} PushConstants;

layout(location = 0) out vec3 outWorldspacePos;
layout(location = 1) out vec3 outWorldspaceNormal;

void main() {
	MeshVertex v = PushConstants.m_globals.m_allGlobals[PushConstants.m_globalIndex].m_vertexBuffer.vertices[gl_VertexIndex];
	vec4 worldSpacePosition = PushConstants.m_instanceTransform * vec4(v.m_positionU0.xyz, 1);
	outWorldspacePos = worldSpacePosition.xyz;
	outWorldspaceNormal = normalize(mat3(transpose(inverse(PushConstants.m_instanceTransform))) * v.m_normalV0.xyz);
	gl_Position = PushConstants.m_globals.m_allGlobals[PushConstants.m_globalIndex].m_projViewTransform * worldSpacePosition;	// clip space
}