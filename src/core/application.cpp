#include "application.h"
#include "SDL_events.h"
#include "SDL_mouse.h"
#include "SDL_scancode.h"
#include "SDL_video.h"
#include "core/camera.h"

#include "imgui.h"
#include "imgui_impl_vulkan.h"
#include "imgui_impl_sdl2.h"

#include <chrono>
#include <SDL.h>

namespace Engine {

Application::Application(): m_renderer(Renderer::getInstance()), m_camera()
{}

void Application::init()
{
	SDL_Init(SDL_INIT_VIDEO);
	SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
	SDL_SetRelativeMouseMode(SDL_FALSE);

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

void Application::run()
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
				handle_mouse_event(e);
			}
			if (e.type == SDL_MOUSEWHEEL) {
				handle_mouse_wheel_event(e);
			}
			if (e.type == SDL_KEYDOWN) {
				handle_keydown_event(e);
			}
			ImGui_ImplSDL2_ProcessEvent(&e);
		}
		process_input(deltaTime);

		ImGui_ImplVulkan_NewFrame();
		ImGui_ImplSDL2_NewFrame();

		ImGui::NewFrame();

		if (m_showImguiWindow) {
			ImGui::ShowDemoWindow();
		}

		//make imgui calculate internal draw structures
		ImGui::Render();
		ImDrawData* drawData = ImGui::GetDrawData();

		m_renderer.draw_frame(m_camera, drawData);
	}
}

void Application::cleanup()
{
	m_renderer.cleanup();
}

void Application::process_input(float deltaTime)
{
	const Uint8* keystate = SDL_GetKeyboardState(NULL);

	if (keystate[SDL_SCANCODE_ESCAPE]) {
		m_quit = true;
	}

	if (m_showImguiWindow) {
		return;
	}

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
}

void Application::handle_mouse_event(const SDL_Event& event)
{
	if (m_showImguiWindow) {
		return;
	}
	float xoffset = event.motion.xrel;
	float yoffset = -event.motion.yrel;

	m_camera.handle_mouse_movement(xoffset, yoffset);
}

void Application::handle_mouse_wheel_event(const SDL_Event& event)
{
	if (m_showImguiWindow) {
		return;
	}
	m_camera.handle_scroll(event.wheel.preciseY);
}

void Application::handle_keydown_event(const SDL_Event& event)
{
	if (event.key.keysym.sym == SDLK_F1) {
		m_showImguiWindow = !m_showImguiWindow;
		if (m_showImguiWindow) {
			SDL_SetRelativeMouseMode(SDL_FALSE);
		} else {
			SDL_SetRelativeMouseMode(SDL_TRUE);
		}
	}

}

int Application::frame_buffer_callback(void* data, SDL_Event* event)
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
}
