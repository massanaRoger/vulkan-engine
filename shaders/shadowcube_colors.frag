
#version 450

#extension GL_GOOGLE_include_directive : require
#include "input_structures_cube.glsl"

layout (location = 0) in vec3 inNormal;
layout (location = 1) in vec3 inColor;
layout (location = 2) in vec3 inWorldPos;
layout (location = 3) in vec2 inUV;
layout (location = 4) in vec3 inFragPos;

layout (location = 0) out vec4 outFragColor;

const float PI = 3.14159265359;
  
float DistributionGGX(vec3 N, vec3 H, float roughness);
float GeometrySchlickGGX(float NdotV, float roughness);
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness);
vec3 fresnelSchlick(float cosTheta, vec3 F0);
float ShadowCalculation(vec4 fragPosLightSpace);


vec3 getNormalFromMap()
{
    if (sceneData.hasNormalMap == 1) {
	vec3 tangentNormal = texture(normalTex, inUV).xyz * 2.0 - 1.0;

	vec3 Q1  = dFdx(inWorldPos);
	vec3 Q2  = dFdy(inWorldPos);
	vec2 st1 = dFdx(inUV);
	vec2 st2 = dFdy(inUV);

	vec3 N   = normalize(inNormal);
	vec3 T  = normalize(Q1 * st2.t - Q2 * st1.t);
	vec3 B  = -normalize(cross(N, T));
	mat3 TBN = mat3(T, B, N);

	return normalize(TBN * tangentNormal);
    } else {
	return normalize(inNormal);  // If no normal map, just use vertex normal
    }
}

float ShadowCalculation(vec3 fragPos)
{
    vec3 fragToLight = inFragPos - sceneData.lightPos[0].xyz;
    float closestDepth = texture(shadowCubeMap, fragToLight).r;
    closestDepth *= sceneData.farPlane;
    float currentDepth = length(fragToLight);
    float shadow = currentDepth > closestDepth ? 1.0 : 0.0;

    return shadow;
}


void main() 
{
	vec3 albedo     = pow(texture(colorTex, inUV).rgb, vec3(2.2));
	float metallic  = texture(metalRoughTex, inUV).b;
	float roughness = texture(metalRoughTex, inUV).g;
	float ao        = texture(aoTex, inUV).r;

	vec3 N = getNormalFromMap();
	vec3 V = normalize(sceneData.camPos.xyz - inWorldPos);

	// calculate reflectance at normal incidence; if dia-electric (like plastic) use F0 
	// of 0.04 and if it's a metal, use the albedo color as F0 (metallic workflow)    
	vec3 F0 = vec3(0.04); 
	F0 = mix(F0, albedo, metallic);

	vec3 Lo = vec3(0.0);
	for (int i = 0; i < 4; i++) {
	    // calculate per-light radiance
	    vec3 L = normalize(sceneData.lightPos[i].xyz - inWorldPos);
	    vec3 H = normalize(V + L);
	    float distance    = length(sceneData.lightPos[i].xyz - inWorldPos);
	    float attenuation = 1.0 / (distance * distance);
	    vec3 radiance     = sceneData.lightColors[i].xyz * attenuation;        

	    // cook-torrance brdf
	    float NDF = DistributionGGX(N, H, roughness);        
	    float G   = GeometrySmith(N, V, L, roughness);      
	    vec3 F    = fresnelSchlick(max(dot(H, V), 0.0), F0);       

	    vec3 numerator    = NDF * G * F;
	    float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
	    vec3 specular     = numerator / denominator;  

	    vec3 kS = F;
	    vec3 kD = vec3(1.0) - kS;
	    kD *= 1.0 - metallic;


	    // add to outgoing radiance Lo
	    float NdotL = max(dot(N, L), 0.0);                
	    Lo += (kD * albedo / PI + specular) * radiance * NdotL;

	}

	// Shadow
	float shadow = ShadowCalculation(inFragPos);

	vec3 ambient = vec3(0.03) * albedo * ao;
	vec3 color = (ambient + (1.0 - shadow)) * Lo;

	color = color / (color + vec3(1.0));
	color = pow(color, vec3(1.0/2.2));  
	outFragColor = vec4(color, 1.0);
}

vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a      = roughness*roughness;
    float a2     = a*a;
    float NdotH  = max(dot(N, H), 0.0);
    float NdotH2 = NdotH*NdotH;
	
    float num   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
	
    return num / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r*r) / 8.0;

    float num   = NdotV;
    float denom = NdotV * (1.0 - k) + k;
	
    return num / denom;
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2  = GeometrySchlickGGX(NdotV, roughness);
    float ggx1  = GeometrySchlickGGX(NdotL, roughness);
	
    return ggx1 * ggx2;
}
