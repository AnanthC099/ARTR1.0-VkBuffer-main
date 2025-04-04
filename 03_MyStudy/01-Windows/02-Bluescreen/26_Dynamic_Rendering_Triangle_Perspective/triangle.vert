#version 450
layout (location = 0) in vec3 inPos;

// Push constant is a mat4
layout(push_constant) uniform PushConstants
{
    mat4 u_projection;
} pc;

void main() 
{
    gl_Position = pc.u_projection * vec4(inPos, 1.0);
}
