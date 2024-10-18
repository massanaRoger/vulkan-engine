#pragma once

#include "glm/ext/vector_float3.hpp"
struct TransformComponent {
	glm::vec3 position;
	glm::vec3 rotation = glm::vec3(0.0f);
	glm::vec3 scale = glm::vec3(1.0f);

	TransformComponent(float x, float y, float z) : position(glm::vec3(x, y, z)) {}

	TransformComponent(
		const glm::vec3& pos = glm::vec3(0.0f),
		const glm::vec3& rot = glm::vec3(0.0f),
		const glm::vec3& scl = glm::vec3(1.0f))
	: position(pos), rotation(rot), scale(scl) {}
};

struct PlayerComponent {
	bool mc;
};

struct MeshComponent {
	std::string meshPath;
};

struct LightComponent {
	glm::vec3 color;
	LightComponent(const glm::vec3& color) : color(color) {}
};

struct CameraComponent {
	float fov;
	float nearClip;
	float farClip;
};
