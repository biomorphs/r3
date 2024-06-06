#version 450
#extension GL_EXT_buffer_reference : require

#include "static_mesh_simple_shared.h"

layout(location = 0) out vec3 outWorldspacePos;
layout(location = 1) out vec3 outWorldspaceNormal;
layout(location = 2) out vec2 outUV;

void main() {
	GlobalConstants globals = PushConstants.m_globals.m_allGlobals[PushConstants.m_globalIndex];
	MeshVertex v = globals.m_vertexBuffer.vertices[gl_VertexIndex];
	vec4 worldSpacePosition = PushConstants.m_instanceTransform * vec4(v.m_positionU0.xyz, 1);
	outWorldspacePos = worldSpacePosition.xyz;
	outWorldspaceNormal = normalize(mat3(transpose(inverse(PushConstants.m_instanceTransform))) * v.m_normalV0.xyz);
	outUV = vec2(v.m_positionU0.w, v.m_normalV0.w);
	gl_Position = globals.m_projViewTransform * worldSpacePosition;	// clip space
}