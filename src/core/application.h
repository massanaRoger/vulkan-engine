#pragma once

#include "SDL_events.h"
#include "SDL_video.h"
#include "core/scene.h"
#include "glm/ext/vector_float3.hpp"
#include "networking/client.h"
#include "vulkan/vk_renderer.h"
#include <cstdint>
#include <string>
#include <unordered_map>

namespace Engine {
class Application {
public:
	Application();
	void run();
	void init();
	void cleanup();
private:
	SDL_Window *m_window;
	VkExtent2D m_windowExtent = { 1700 , 900 };
	Renderer& m_renderer;
	Camera m_camera;
	Scene m_scene;
	Client m_client;
	uint64_t m_UUID;

	std::unordered_map<std::string, glm::vec3> m_playersData;

	bool m_quit = false;
	bool m_showImguiWindow = true;

	void process_input(float time);
	void handle_mouse_event(const SDL_Event& event);
	void handle_mouse_wheel_event(const SDL_Event& event);
	void handle_keydown_event(const SDL_Event& event);

	static int frame_buffer_callback(void* data, SDL_Event* event);
};
}
