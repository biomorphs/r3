#version 450
#extension GL_EXT_buffer_reference : require

#include "mesh_render_shared.h"
#include "utils.h"

layout(location = 0) out vec3 outWorldspacePos;
layout(location = 1) out vec3 outWorldspaceNormal;
layout(location = 2) out vec2 outUV;
layout(location = 3) out flat uint instanceIndex;
layout(location = 4) out mat3 outTBN;


void main() {
	GlobalConstants globals = PushConstants.m_globals.data[0];
	MeshInstanceData thisInstance = PushConstants.m_instances.data[gl_InstanceIndex];
	MeshMaterial myMaterial = thisInstance.m_material.data[0];
	MeshVertex v = globals.m_vertexBuffer.data[gl_VertexIndex];
	vec4 worldSpacePosition = thisInstance.m_transform * vec4(v.m_positionU0.xyz, 1);
	outWorldspacePos = worldSpacePosition.xyz;
	outWorldspaceNormal = normalize(mat3(transpose(inverse(thisInstance.m_transform))) * v.m_normalV0.xyz);
	outUV = myMaterial.m_uvOffsetScale.xy + vec2(v.m_positionU0.w * myMaterial.m_uvOffsetScale.z, v.m_normalV0.w * myMaterial.m_uvOffsetScale.w);
	instanceIndex = gl_InstanceIndex;
	outTBN = CalculateTBN(thisInstance.m_transform, v.m_tangentPad.xyz, v.m_normalV0.xyz);
	gl_Position = globals.m_projViewTransform * worldSpacePosition;	// clip space
}