#version 450 core
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 vPosition;   // <— was vec4, but your attrib is R32G32B32
layout(location = 1) in vec3 vColor;

layout(location = 0) out vec3 outColor;

layout(binding = 0) uniform mvpMatrix {
    mat4 modelMatrix;
    mat4 viewMatrix;
    mat4 projectionMatrix;
} uMVP;

void main(void)
{
    gl_Position = uMVP.projectionMatrix * uMVP.viewMatrix * uMVP.modelMatrix * vec4(vPosition, 1.0);
    outColor = vColor;
}
