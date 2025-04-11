#version 450

// Even though 'fragColor' is declared, we don't use it.
// If you don't need the color at all, you can remove references
// to it in the vertex shader and pipeline as well.
layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 1) uniform sampler2D myTexture;

void main()
{
    // Sample the texture only, ignoring the vertex color
    vec4 texSample = texture(myTexture, fragTexCoord);
    outColor = texSample;
}
