#version 450

layout (location = 0) in vec2 inPos;
layout (location = 1) in vec2 inTexCoords;

layout (location = 0) out vec2 outTexCoords;

void main()
{
    outTexCoords = inTexCoords;
    gl_Position = vec4(inPos, 1.0, 1.0);
}
