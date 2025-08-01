#version 450 core
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 outColor;
layout(location = 0) out vec4 FragColor;

void main(void)
{
	FragColor = vec4(outColor, 1.0);
}
