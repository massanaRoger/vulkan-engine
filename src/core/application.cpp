#include "application.h"
#include "SDL_events.h"
#include "SDL_video.h"
#include <SDL.h>

Engine::Application::Application(): m_renderer(Renderer())
{}

void Engine::Application::init()
{
	SDL_Init(SDL_INIT_VIDEO);
	SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_VULKAN);

	m_window = SDL_CreateWindow(
		"Vulkan Engine",
		SDL_WINDOWPOS_UNDEFINED,
		SDL_WINDOWPOS_UNDEFINED,
		m_windowExtent.width,
		m_windowExtent.height,
		window_flags
	);
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

	SDL_DestroyWindow(m_window);
	m_renderer.cleanup();
}
