#include "camera.h"
#include "glm/ext/matrix_transform.hpp"

glm::mat4 Engine::Camera::get_view_matrix() const
{
	return glm::lookAt(position, position + forward, up);
}

