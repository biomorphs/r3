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
	Globals globals = PushConstants.m_globals.data[0];
	MeshInstanceData thisInstance = PushConstants.m_instances.data[gl_InstanceIndex];
	MeshMaterial myMaterial = thisInstance.m_material.data[0];
	vec3 inPos = globals.m_vertexPosUVBuffer.data[gl_VertexIndex].m_position;
	vec3 inNormal = vec3(globals.m_vertexNormTangentBuffer.data[gl_VertexIndex].m_normal[0], 
						 globals.m_vertexNormTangentBuffer.data[gl_VertexIndex].m_normal[1], 
						 globals.m_vertexNormTangentBuffer.data[gl_VertexIndex].m_normal[2]);
	vec3 inTangent = vec3(globals.m_vertexNormTangentBuffer.data[gl_VertexIndex].m_tangent[0],
						  globals.m_vertexNormTangentBuffer.data[gl_VertexIndex].m_tangent[1],
						  globals.m_vertexNormTangentBuffer.data[gl_VertexIndex].m_tangent[2]);
	vec2 inUV = vec2(globals.m_vertexPosUVBuffer.data[gl_VertexIndex].m_uv[0],
					 globals.m_vertexPosUVBuffer.data[gl_VertexIndex].m_uv[1]);
	vec4 worldSpacePosition = thisInstance.m_transform * vec4(inPos, 1);
	outWorldspacePos = worldSpacePosition.xyz;
	outWorldspaceNormal = normalize(mat3(transpose(inverse(thisInstance.m_transform))) * inNormal);
	outUV = myMaterial.m_uvOffsetScale.xy + vec2(inUV.x * myMaterial.m_uvOffsetScale.z, inUV.y * myMaterial.m_uvOffsetScale.w);
	instanceIndex = gl_InstanceIndex;
	outTBN = CalculateTBN(thisInstance.m_transform, inTangent, inNormal);
	gl_Position = globals.m_projViewTransform * worldSpacePosition;
}