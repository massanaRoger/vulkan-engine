layout(set = 0, binding = 0) uniform SceneData {   
	mat4 view;
	mat4 proj;
	mat4 viewproj;
	mat4 lightSpaceMatrix;
	vec4 ambientColor;
	vec4 sunlightDirection; //w for sun power
	vec4 sunlightColor;
	vec4 camPos;
	vec4 lightPos[4];
	vec4 lightColors[4];
	int hasNormalMap;
	float farPlane;
} sceneData;

layout(set = 1, binding = 0) uniform GLTFMaterialData{   
	vec4 colorFactors;
	vec4 metal_rough_factors;
} materialData;

layout(set = 1, binding = 1) uniform sampler2D colorTex;
layout(set = 1, binding = 2) uniform sampler2D metalRoughTex;
layout(set = 1, binding = 3) uniform sampler2D normalTex;
layout(set = 1, binding = 4) uniform sampler2D aoTex;
layout(set = 1, binding = 5) uniform samplerCube shadowCubeMap;
