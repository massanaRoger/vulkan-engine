#include "scene.h"
#include "core/components.h"
#include "glm/ext/vector_float3.hpp"

namespace Engine {

entt::entity Scene::create_entity(const std::string& name)
{
	entt::entity entity = m_registry.create();
	m_entityNames[entity] = name;
	return entity;
}

void Scene::destroy_entity(entt::entity entity)
{
	m_registry.destroy(entity);
	m_entityNames.erase(entity);
}

void Scene::load_scene(const std::string& filePath){}
void Scene::save_scene(const std::string& filePath){}

entt::registry& Scene::get_registry()
{
	return m_registry;
}

void create_example_scene(Scene& scene)
{
	auto& registry = scene.get_registry();

	entt::entity helmet1 = scene.create_entity("Helmet 1");
	registry.emplace<TransformComponent>(helmet1, glm::vec3(0.0f, 0.0f, 0.0f));
	registry.emplace<MeshComponent>(helmet1, "../../models/DamagedHelmet.glb");

	entt::entity helmet2 = scene.create_entity("Helmet 2");
	registry.emplace<TransformComponent>(helmet2, glm::vec3(0.0f, 0.0f, 5.0f));
	registry.emplace<MeshComponent>(helmet2, "../../models/DamagedHelmet.glb");

	entt::entity light = scene.create_entity("Directional Light");
	registry.emplace<TransformComponent>(light, glm::vec3(0.0f, 0.0f, 10.0f));
	registry.emplace<LightComponent>(light, glm::vec3(150.0f, 150.0f, 150.0f));

	entt::entity camera = scene.create_entity("Main Camera");
	registry.emplace<TransformComponent>(camera, glm::vec3(0.0f, 2.0f, 5.0f));
	registry.emplace<CameraComponent>(camera, 45.0f, 0.1f, 100.0f);
}

}
