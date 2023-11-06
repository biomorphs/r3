#version 450

layout(location = 0) in vec4 inPosition;
layout(location = 1) in vec4 inColour;

layout(location = 0) out vec4 fragColor;

void main() {
    gl_Position = inPosition;
    fragColor = inColour;
}