#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference : require

#include "input_structures_cube.glsl"

layout (location = 0) out vec3 outNormal;
layout (location = 1) out vec3 outColor;
layout (location = 2) out vec3 outWorldPos;
layout (location = 3) out vec2 outUV;
layout (location = 4) out vec3 outFragPos;

struct Vertex {
	vec3 position;
	float uv_x;
	vec3 normal;
	float uv_y;
	vec4 color;
}; 

layout(buffer_reference, std430) readonly buffer VertexBuffer{ 
	Vertex vertices[];
};

//push constants block
layout( push_constant ) uniform constants
{
	mat4 renderMatrix;
	mat4 lightSpaceMatrix;
	VertexBuffer vertexBuffer;
} PushConstants;

void main() 
{
	Vertex v = PushConstants.vertexBuffer.vertices[gl_VertexIndex];
	
	vec4 position = vec4(v.position, 1.0f);

	gl_Position =  sceneData.viewproj * PushConstants.renderMatrix * position;

	outNormal = (PushConstants.renderMatrix * vec4(v.normal, 0.f)).xyz;
	outWorldPos = (PushConstants.renderMatrix * position).xyz / (PushConstants.renderMatrix * position).w;
	outColor = v.color.xyz * materialData.colorFactors.xyz;
	outUV.x = v.uv_x;
	outUV.y = v.uv_y;
	outFragPos = vec3(PushConstants.renderMatrix * position);
}
