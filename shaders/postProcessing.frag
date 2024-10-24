#version 450

layout (location = 0) out vec4 outFragColor;

layout (location = 0) in vec2 inTexCoords;

layout(set = 0, binding = 0) uniform sampler2D colorImage;

void main()
{
	vec3 hdrColor = texture(colorImage, inTexCoords).rgb;
	vec3 result = vec3(1.0) - exp(-hdrColor);
	outFragColor = vec4(result, 1.0);
}
