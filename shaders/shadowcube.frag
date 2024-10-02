#version 450

layout (location = 0) out float outFragColor;

layout (location = 0) in vec4 inPos;

layout(set = 0, binding = 0) uniform SceneData {   
	vec4 lightPos;
	float farPlane;
} sceneData;

void main() 
{
	float lightDistance = length(inPos.xyz - sceneData.lightPos.xyz);
	lightDistance = lightDistance / sceneData.farPlane;

	outFragColor = lightDistance;
}
