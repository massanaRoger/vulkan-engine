#pragma once

#include "entt.hpp"
#include <cstdint>
#include <unordered_map>

namespace Engine {


class Scene {
public:
	entt::entity create_entity(const std::string& name);
	void destroy_entity(entt::entity name);

	void load_scene(const std::string& filePath, uint64_t uuid);
	void save_scene(const std::string& filePath);

	void update_positions(std::unordered_map<uint64_t, glm::vec3>& positions, uint64_t uuid);

	entt::registry& get_registry();
	std::unordered_map<uint64_t, entt::entity> uuids;

private:
	entt::registry m_registry;
	std::unordered_map<entt::entity, std::string> m_entityNames;
};


}
