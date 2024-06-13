#version 450
#extension GL_EXT_buffer_reference : require

#include "static_mesh_simple_shared.h"
#include "utils.h"

layout(location = 0) out vec3 outWorldspacePos;
layout(location = 1) out vec3 outWorldspaceNormal;
layout(location = 2) out vec2 outUV;
layout(location = 3) out mat3 outTBN;

void main() {
	GlobalConstants globals = AllGlobals[PushConstants.m_globalIndex];
	MeshVertex v = globals.m_vertexBuffer.vertices[gl_VertexIndex];
	vec4 worldSpacePosition = PushConstants.m_instanceTransform * vec4(v.m_positionU0.xyz, 1);
	outWorldspacePos = worldSpacePosition.xyz;
	outWorldspaceNormal = normalize(mat3(transpose(inverse(PushConstants.m_instanceTransform))) * v.m_normalV0.xyz);
	outUV = vec2(v.m_positionU0.w, v.m_normalV0.w);
	outTBN = CalculateTBN(PushConstants.m_instanceTransform, v.m_tangentPad.xyz, v.m_normalV0.xyz);
	gl_Position = globals.m_projViewTransform * worldSpacePosition;	// clip space
}