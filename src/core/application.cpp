#include "application.h"
#include "SDL_events.h"
#include "SDL_mouse.h"
#include "SDL_video.h"
#include "core/camera.h"

#include <chrono>
#include <SDL.h>
#include <iostream>

Engine::Application::Application(): m_renderer(Renderer::getInstance()), m_camera()
{}

void Engine::Application::init()
{
	SDL_Init(SDL_INIT_VIDEO);
	SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
	SDL_SetRelativeMouseMode(SDL_TRUE);

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
	float deltaTime = 0.0f;
	auto lastFrame = std::chrono::high_resolution_clock::now();
	while(!m_quit) {
		auto currentFrame = std::chrono::high_resolution_clock::now();
		deltaTime = std::chrono::duration<float, std::chrono::seconds::period>(currentFrame - lastFrame).count();
		lastFrame = currentFrame;
		while (SDL_PollEvent(&e) != 0) {
			if (e.type == SDL_QUIT) {
				m_quit = true;
			}
			if (e.type == SDL_MOUSEMOTION) {
				handle_mouse_events(e);
			}
		}
		process_input(deltaTime);
		m_renderer.draw_frame(m_camera);
	}
}

void Engine::Application::cleanup()
{
	m_renderer.cleanup();
}

void Engine::Application::process_input(float deltaTime)
{
	const Uint8* keystate = SDL_GetKeyboardState(NULL);

	if (keystate[SDL_SCANCODE_W]) {
		m_camera.handle_keyboard_movement(Directions::Top, deltaTime);
	}
	if (keystate[SDL_SCANCODE_S]) {
		m_camera.handle_keyboard_movement(Directions::Bottom, deltaTime);
	}
	if (keystate[SDL_SCANCODE_A]) {
		m_camera.handle_keyboard_movement(Directions::Left, deltaTime);
	}
	if (keystate[SDL_SCANCODE_D]) {
		m_camera.handle_keyboard_movement(Directions::Right, deltaTime);
	}
	if (keystate[SDL_SCANCODE_ESCAPE]) {
		m_quit = true;
	}
}

void Engine::Application::handle_mouse_events(const SDL_Event& event)
{
	float xoffset = event.motion.xrel;
	float yoffset = -event.motion.yrel;

	m_camera.handle_mouse_movement(xoffset, yoffset);
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

