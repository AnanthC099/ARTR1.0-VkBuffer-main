#version 460 core

// Simple 2D position
layout(location = 0) in vec2 inPosition;

// We’ll push an orthographic matrix via push constants
layout(push_constant) uniform PushConstants {
    mat4 ortho;
} pc;

void main()
{
    // Multiply the inPosition by the push-constant ortho matrix.
    // We treat Z as 0 and W as 1 for a simple 2D triangle.
    gl_Position = pc.ortho * vec4(inPosition, 0.0, 1.0);
}
