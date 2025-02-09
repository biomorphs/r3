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
	MeshVertex v = globals.m_vertexBuffer.data[gl_VertexIndex];
	mat4 instanceTransform = thisInstance.m_transform;
	vec4 worldSpacePosition = instanceTransform * vec4(v.m_positionU0.xyz, 1);
	outUV = myMaterial.m_uvOffsetScale.xy + vec2(v.m_positionU0.w * myMaterial.m_uvOffsetScale.z, v.m_normalV0.w * myMaterial.m_uvOffsetScale.w);
	instanceIndex = gl_InstanceIndex;
	gl_Position = globals.m_projViewTransform * worldSpacePosition;
}