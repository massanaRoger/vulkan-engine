#pragma once

#include "SDL_events.h"
#include "SDL_video.h"
#include "vulkan/vk_renderer.h"

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

	static int frame_buffer_callback(void* data, SDL_Event* event);
};
}
