#version 450

layout (location = 0) in vec2 inPosition;
layout (location = 1) in vec3 inColor;

// Our uniform buffer object for projection
layout(std140, binding = 0) uniform UBO
{
    mat4 proj;
} ubo;

layout (location = 0) out vec3 outColor;

void main()
{
    // Multiply the 2D vertex by our projection matrix
    gl_Position = ubo.proj * vec4(inPosition, 0.0, 1.0);
    outColor = inColor;
}
