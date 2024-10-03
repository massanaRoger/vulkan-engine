#version 450

layout (location = 0) out float outFragColor;

layout (location = 0) in vec4 inPos;
layout (location = 1) in vec2 inUV;
layout (location = 2) in float inAlphaCutoff;

layout(set = 0, binding = 0) uniform SceneData {   
	vec4 lightPos;
	float farPlane;
} sceneData;

layout(set = 1, binding = 1) uniform sampler2D colorTex;

void main() 
{
	float alpha = texture(colorTex, inUV).a;

	if (alpha < inAlphaCutoff) {
		discard;
	}

	float lightDistance = length(inPos.xyz - sceneData.lightPos.xyz);
	lightDistance = lightDistance / sceneData.farPlane;

	outFragColor = lightDistance;
}
