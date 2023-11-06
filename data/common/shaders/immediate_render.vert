#version 450

layout(location = 0) in vec4 inPosition;
layout(location = 1) in vec4 inColour;

// transform set in push constants for now (laziness)
layout(push_constant) uniform constants
{
	mat4 m_transform;
} PushConstants;

layout(location = 0) out vec4 fragColor;

void main() {
    gl_Position = inPosition * PushConstants.m_transform;
    fragColor = inColour;
}