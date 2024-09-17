#pragma once

#include "entt.hpp"
#include <unordered_map>

namespace Engine {


class Scene {
public:
	entt::entity create_entity(const std::string& name);
	void destroy_entity(entt::entity name);

	void load_scene(const std::string& filePath);
	void save_scene(const std::string& filePath);

	entt::registry& get_registry();

private:
	entt::registry m_registry;
	std::unordered_map<entt::entity, std::string> m_entityNames;
};

void create_example_scene(Scene& scene);

}
