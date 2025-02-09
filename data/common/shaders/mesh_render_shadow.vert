#version 450
#extension GL_EXT_buffer_reference : require

#include "mesh_render_shared.h"
#include "utils.h"

layout(location = 0) out vec2 outUV;
layout(location = 1) out flat uint instanceIndex;


void main() {
	Globals globals = PushConstants.m_globals.data[0];
	MeshInstanceData thisInstance = PushConstants.m_instances.data[gl_InstanceIndex];
	MeshMaterial myMaterial = thisInstance.m_material.data[0];
	vec3 inPos = globals.m_vertexPosUVBuffer.data[gl_VertexIndex].m_position;
	vec2 inUV = vec2(globals.m_vertexPosUVBuffer.data[gl_VertexIndex].m_uv[0],
					 globals.m_vertexPosUVBuffer.data[gl_VertexIndex].m_uv[1]);
	mat4 instanceTransform = thisInstance.m_transform;
	vec4 worldSpacePosition = instanceTransform * vec4(inPos, 1);
	outUV = myMaterial.m_uvOffsetScale.xy + vec2(inUV.x * myMaterial.m_uvOffsetScale.z, inUV.y * myMaterial.m_uvOffsetScale.w);
	instanceIndex = gl_InstanceIndex;
	gl_Position = globals.m_projViewTransform * worldSpacePosition;
}