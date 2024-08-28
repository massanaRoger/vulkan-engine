#pragma once

namespace Engine {

enum class Directions {
	Top,
	Right,
	Left,
	Bottom
};

class Camera {
public:
	glm::vec3 position = glm::vec3(0.0f, 0.0f, 3.0f);
	glm::vec3 forward = glm::vec3(0.0f, 0.0f, -1.0f);
	glm::vec3 up = glm::vec3(0.0f, 1.0f,  0.0f);
	float yaw = -90.0f;
	float pitch = 0.0f;

	[[nodiscard]] glm::mat4 get_view_matrix() const; 
	void handle_mouse_movement(float xoffset, float yoffset);
	void handle_keyboard_movement(Directions direction, float deltaTime);
};

}
