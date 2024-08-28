#include "camera.h"
#include "glm/ext/matrix_transform.hpp"

namespace Engine {

glm::mat4 Camera::get_view_matrix() const
{
	return glm::lookAt(position, position + forward, up);
}

void Camera::handle_mouse_movement(float xoffset, float yoffset)
{

	const float sensitivity = 0.1f;
	xoffset *= sensitivity;
	yoffset *= sensitivity;

	yaw += xoffset;
	pitch += yoffset;

	yaw = fmod(yaw + 360.0f, 360.0f);

	if (pitch > 89.0f) {
		pitch = 89.0f;
	}
	if (pitch < -89.0f) {
		pitch = -89.0f;
	}

	glm::vec3 direction;
	direction.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
	direction.y = sin(glm::radians(pitch));
	direction.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
	forward = glm::normalize(direction);
}

void Camera::handle_keyboard_movement(Directions direction, float deltaTime)
{
	const float cameraSpeed = 1.5f * deltaTime;
	if (direction == Directions::Top) {
		position += cameraSpeed * forward;
	}
	if (direction == Directions::Bottom) {
		position -= cameraSpeed * forward;
	}
	if (direction == Directions::Left) {

		position -= glm::normalize(glm::cross(forward, up)) * cameraSpeed;
	}
	if (direction == Directions::Right) {

		position += glm::normalize(glm::cross(forward, up)) * cameraSpeed;
	}
}

}
