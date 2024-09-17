#pragma once

struct TransformComponent {
	glm::vec3 position;
	glm::vec3 rotation;
	glm::vec3 scale;

	TransformComponent(
		const glm::vec3& pos = glm::vec3(0.0f),
		const glm::vec3& rot = glm::vec3(0.0f),
		const glm::vec3& scl = glm::vec3(1.0f))
	: position(pos), rotation(rot), scale(scl) {}
};

struct MeshComponent {
	std::string meshPath;
};

struct LightComponent {
	glm::vec3 color;
};

struct CameraComponent {
	float fov;
	float nearClip;
	float farClip;
};
