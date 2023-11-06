#version 450

// vertex data
layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec3 inColour;

// push constants
layout(push_constant) uniform constants
{
	vec4 m_colour;
	mat4 m_matrix;
} PushConstants;

layout(location = 0) out vec3 fragColor;

void main() {
    gl_Position = PushConstants.m_matrix * vec4(inPosition, 0.0, 1.0);
    fragColor = inColour * vec3(PushConstants.m_colour);
}