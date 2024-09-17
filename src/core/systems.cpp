#include "systems.h"
#include "core/components.h"

namespace Engine {

void update_transforms(Scene& scene)
{
	auto& registry = scene.get_registry();

	auto view = registry.view<TransformComponent>();
	for (auto entity : view) {
		auto& _ = view.get<TransformComponent>(entity);
		// TODO: Add physics, animations, etc.
	}
}
void render_scene(Scene& scene) {}
}

