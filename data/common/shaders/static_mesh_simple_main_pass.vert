#version 450
#extension GL_EXT_buffer_reference : require

#include "mesh_data.h"

layout(buffer_reference, std430) readonly buffer VertexBuffer { 
	MeshVertex vertices[];
};

// transform and vertex buffer set in push constants
layout(push_constant) uniform constants
{
	mat4 m_transform;
	VertexBuffer vertexBuffer;
} PushConstants;

layout(location = 0) out vec4 fragColor;

void main() {
	MeshVertex v = PushConstants.vertexBuffer.vertices[gl_VertexIndex];
    gl_Position = PushConstants.m_transform * vec4(v.m_positionU0.xyz, 1);
    fragColor = vec4(v.m_normalV0.xyz,1);
}