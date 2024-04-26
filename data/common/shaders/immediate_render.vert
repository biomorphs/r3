#version 450
#extension GL_EXT_buffer_reference : require

struct Vertex {
	vec4 position;
	vec4 colour;
}; 
layout(buffer_reference, std430) readonly buffer VertexBuffer { 
	Vertex vertices[];
};

// transform and vertex buffer set in push constants
layout(push_constant) uniform constants
{
	mat4 m_transform;
	VertexBuffer vertexBuffer;
} PushConstants;

layout(location = 0) out vec4 fragColor;

void main() {
	Vertex v = PushConstants.vertexBuffer.vertices[gl_VertexIndex];
    gl_Position = PushConstants.m_transform * v.position;
    fragColor = v.colour;
}