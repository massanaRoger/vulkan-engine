#pragma once

namespace Engine {

class Camera {
public:
	[[nodiscard]] glm::mat4 get_view_matrix() const; 

	glm::vec3 position = glm::vec3(0.0f, 0.0f, 3.0f);
	glm::vec3 forward = glm::vec3(0.0f, 0.0f, -1.0f);
	glm::vec3 up = glm::vec3(0.0f, 1.0f,  0.0f);
};

}
