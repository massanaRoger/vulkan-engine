#include "scene.h"
#include "core/components.h"
#include "core/types.h"
#include "glm/ext/vector_float3.hpp"
#include "json.hpp"
#include "vulkan/vk_renderer.h"
#include <cstdint>
#include <fstream>
#include <unordered_map>
#include <vector>

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

void Scene::update_positions(std::unordered_map<uint64_t, glm::vec3>& positions, uint64_t uuid)
{
	for (const auto& [k, pos] : positions) {
		if (!uuids.contains(k)) {
			entt::entity entity = m_registry.create();
			uuids[k] = entity;
			m_registry.emplace<TransformComponent>(entity, pos.x, pos.y, pos.z);
			if (k == uuid) {
				m_registry.emplace<PlayerComponent>(entity, true);
			} else {
				m_registry.emplace<PlayerComponent>(entity, false);
			}
		} else {
			auto& transform = m_registry.get<TransformComponent>(uuids[k]);
			transform.position = { pos.x, pos.y, pos.z };
		}

	}
}

void Scene::load_scene(const std::string& filePath, uint64_t uuid)
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

}
