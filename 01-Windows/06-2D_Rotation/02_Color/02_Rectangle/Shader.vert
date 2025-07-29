#version 450 core
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in  vec4 vPosition;
layout(location = 1) in  vec3 vColor;

layout(location = 0) out vec3 oColor;

layout(binding = 0) uniform mvpMatrix
{
    mat4 modelMatrix;
    mat4 viewMatrix;
    mat4 projectionMatrix;
} uMVP;

void main(void)
{
    gl_Position = uMVP.projectionMatrix *
                  uMVP.viewMatrix *
                  uMVP.modelMatrix *
                  vPosition;
    oColor = vColor;
}
