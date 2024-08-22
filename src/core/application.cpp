#include "application.h"
#include "SDL_events.h"
#include "SDL_video.h"
#include <SDL.h>

Engine::Application::Application(): m_renderer(Renderer::getInstance())
{}

void Engine::Application::init()
{
	SDL_Init(SDL_INIT_VIDEO);
	SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);

	m_window = SDL_CreateWindow(
		"Vulkan Engine",
		SDL_WINDOWPOS_UNDEFINED,
		SDL_WINDOWPOS_UNDEFINED,
		m_windowExtent.width,
		m_windowExtent.height,
		window_flags
	);
	SDL_AddEventWatch(frame_buffer_callback, m_window);
	m_renderer.init_vulkan(m_window);
}

void Engine::Application::run()
{
	SDL_Event e;
	bool quit = false;
	while(!quit) {
		while (SDL_PollEvent(&e) != 0) {
			if (e.type == SDL_QUIT) {
				quit = true;
			}
		}
		m_renderer.draw_frame();
	}

	m_renderer.cleanup();
}

int Engine::Application::frame_buffer_callback(void* data, SDL_Event* event)
{
	if (event->type == SDL_WINDOWEVENT &&
		event->window.event == SDL_WINDOWEVENT_RESIZED) {
		SDL_Window* win = SDL_GetWindowFromID(event->window.windowID);
		if (win == (SDL_Window*)data) {
			Renderer& rend = Renderer::getInstance();
			rend.frameBufferResized = true;
		}
	}

	return 0;
}
