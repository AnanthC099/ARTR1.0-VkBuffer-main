#version 450

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragTexCoord;

layout(set = 0, binding = 0) uniform UBO {
    mat4 mvp;
} ubo;

void main()
{
    gl_Position = ubo.mvp * vec4(inPos, 1.0);
    fragColor   = inColor;
    fragTexCoord = inTexCoord;
}
