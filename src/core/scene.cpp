#include "scene.h"
#include "core/components.h"
#include "glm/ext/vector_float3.hpp"
#include "json.hpp"
#include "vulkan/vk_renderer.h"
#include <fstream>

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

void Scene::load_scene(const std::string& filePath)
{
	std::ifstream file(filePath);
	nlohmann::json jsonData;
	file >> jsonData;

	for (const auto& entityData : jsonData["entities"]) {
		entt::entity entity = create_entity(entityData["name"]);

		if (entityData.contains("transform")) {
			glm::vec3 position = {entityData["transform"]["position"][0], entityData["transform"]["position"][1], entityData["transform"]["position"][2]};
			glm::vec3 rotation = {entityData["transform"]["rotation"][0], entityData["transform"]["rotation"][1], entityData["transform"]["rotation"][2]};
			glm::vec3 scale = {entityData["transform"]["scale"][0], entityData["transform"]["scale"][1], entityData["transform"]["scale"][2]};
			m_registry.emplace<TransformComponent>(entity, position, rotation, scale);
		}

		if (entityData.contains("mesh")) {
			m_registry.emplace<MeshComponent>(entity, entityData["mesh"]["path"]);
		}

		if (entityData.contains("light")) {
			glm::vec3 color = {entityData["light"]["color"][0], entityData["light"]["color"][1], entityData["light"]["color"][2]};
			m_registry.emplace<LightComponent>(entity, color);
		}
	}

	Renderer::getInstance().update_loaded_scenes(*this);
}

void Scene::save_scene(const std::string& filePath)
{
	nlohmann::json jsonData;

	for (auto entity : m_registry.view<entt::entity>()) {
		nlohmann::json entityData;

		entityData["name"] = m_entityNames[entity];

		if (m_registry.all_of<TransformComponent>(entity)) {
			auto& transform = m_registry.get<TransformComponent>(entity);
			entityData["transform"]["position"] = {transform.position.x, transform.position.y, transform.position.z};
			entityData["transform"]["rotation"] = {transform.rotation.x, transform.rotation.y, transform.rotation.z};
			entityData["transform"]["scale"] = {transform.scale.x, transform.scale.y, transform.scale.z};
		}

		if (m_registry.all_of<MeshComponent>(entity)) {
			auto& mesh = m_registry.get<MeshComponent>(entity);
			entityData["mesh"]["path"] = mesh.meshPath;
		}

		if (m_registry.all_of<LightComponent>(entity)) {
			auto& light = m_registry.get<LightComponent>(entity);
			entityData["light"]["color"] = {light.color.x, light.color.y, light.color.z};
		}

		jsonData["entities"].push_back(entityData);
	}

	std::ofstream file(filePath);
	file << jsonData.dump(4);
	file.close();
}

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
