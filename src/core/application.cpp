#include "application.h"
#include "SDL_events.h"
#include "SDL_video.h"
#include "core/camera.h"

#include <chrono>
#include <SDL.h>

Engine::Application::Application(): m_renderer(Renderer::getInstance()), m_camera()
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
	const float cameraSpeed = 1.5f * deltaTime;
	const Uint8* keystate = SDL_GetKeyboardState(NULL);

	if (keystate[SDL_SCANCODE_W]) {
		m_camera.position += cameraSpeed * m_camera.forward;
	}
	if (keystate[SDL_SCANCODE_S]) {
		m_camera.position -= cameraSpeed * m_camera.forward;
	}
	if (keystate[SDL_SCANCODE_A]) {
		m_camera.position -= glm::normalize(glm::cross(m_camera.forward, m_camera.up)) * cameraSpeed;
	}
	if (keystate[SDL_SCANCODE_D]) {
		m_camera.position += glm::normalize(glm::cross(m_camera.forward, m_camera.up)) * cameraSpeed;
	}
	if (keystate[SDL_SCANCODE_ESCAPE]) {
		m_quit = true;
	}
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

