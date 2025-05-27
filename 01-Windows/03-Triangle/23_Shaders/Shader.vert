#version 450 core
#EXTENSION GL_ARB_separate_shader_objects : enable
layout(location = 0) in vec4 vPosition;
void main(void)
{
	/*
	Code
	*/
	gl_position = vPosition;
}

