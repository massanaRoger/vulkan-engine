#version 450
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference : require

layout (location = 0) out vec4 outFragPos;
layout (location = 1) out vec2 outUV;
layout (location = 2) out float outAlphaCutoff;

layout(set = 0, binding = 0) uniform SceneData {   
	vec4 lightPos;
	float farPlane;
} sceneData;

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

layout( push_constant ) uniform constants
{
	mat4 render_matrix;
	mat4 lightSpaceMatrix;
	VertexBuffer vertexBuffer;
	float alphaCutoff;
} PushConstants;


void main()
{
	Vertex v = PushConstants.vertexBuffer.vertices[gl_VertexIndex];
	outFragPos = PushConstants.render_matrix * vec4(v.position, 1.0);
	outUV.x = v.uv_x;
	outUV.y = v.uv_y;
	outAlphaCutoff = PushConstants.alphaCutoff;
	gl_Position = PushConstants.lightSpaceMatrix * outFragPos;
}
