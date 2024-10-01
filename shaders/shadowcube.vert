#version 450
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference : require

layout (location = 0) out vec4 outPos;
layout (location = 1) out vec3 outLightPos;

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
} PushConstants;

layout(set = 0, binding = 0) uniform SceneData {   
	mat4 view;
	mat4 proj;
	mat4 model;
	vec4 lightPos;
} sceneData;

void main()
{
    Vertex v = PushConstants.vertexBuffer.vertices[gl_VertexIndex];
    gl_Position = sceneData.proj * PushConstants.lightSpaceMatrix * sceneData.model * vec4(v.position, 1.0);

    outPos = vec4(v.position, 1.0);
    outLightPos = sceneData.lightPos.xyz;
}
