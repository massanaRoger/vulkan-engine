#include "application.h"
#include "SDL_events.h"
#include "SDL_mouse.h"
#include "SDL_scancode.h"
#include "SDL_video.h"
#include "core/camera.h"

#include "core/scene.h"
#include "imgui.h"
#include "imgui_impl_vulkan.h"
#include "imgui_impl_sdl2.h"

#include <chrono>
#include <SDL.h>
#include <iostream>

namespace Engine {

Application::Application(): m_renderer(Renderer::getInstance())
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
	m_renderer.init_vulkan(m_window, &m_scene);
	auto error = m_client.init("127.0.0.1");
	if (error.code != Client::ErrorCode::Success) {
		std::cerr << error.message << std::endl;
	}
	m_UUID = m_client.wait_for_uuid();
}

void Application::run()
{
	SDL_Event e;
	float deltaTime = 0.0f;
	auto lastFrame = std::chrono::high_resolution_clock::now();
	while(!m_quit) {
		auto positions = m_client.get_current_positions();
		m_scene.update_positions(positions, m_UUID);

		auto start = std::chrono::system_clock::now();
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

		ImGui::Begin("Controls");

		ImGui::Text("F1: Toggle cursor");
		ImGui::Text("ESC: Exit");

		ImGui::Text("Stats");
		ImGui::Text("frametime %f ms", m_renderer.stats.frameTime);
		ImGui::Text("draw time %f ms", m_renderer.stats.meshDrawTime);
		ImGui::Text("update time %f ms", m_renderer.stats.sceneUpdateTime);
		ImGui::Text("triangles %i", m_renderer.stats.triangleCount);
		ImGui::Text("draws %i", m_renderer.stats.drawcallCount);

		static char loadFilePath[128] = "../../scenes/scene.json";
		ImGui::Text("Load scene");
		ImGui::InputTextWithHint("File path", "Enter text here", loadFilePath, IM_ARRAYSIZE(loadFilePath));
		if (ImGui::Button("Load")) {
			m_scene.load_scene(loadFilePath, m_UUID);
		}

		static char saveFilePath[128] = "../../scenes/scene.json";
		ImGui::Text("Save scene");
		ImGui::InputTextWithHint("File path", "Enter text here", saveFilePath, IM_ARRAYSIZE(saveFilePath));

		if (ImGui::Button("Save")) {
			m_scene.save_scene(saveFilePath);
		}

		ImGui::End();

		if (m_showImguiWindow) {
		}

		//make imgui calculate internal draw structures
		ImGui::Render();
		ImDrawData* drawData = ImGui::GetDrawData();

		m_renderer.draw_frame(m_camera, drawData);

		auto end = std::chrono::system_clock::now();

		//convert to microseconds (integer), and then come back to miliseconds
		auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
		m_renderer.stats.frameTime = elapsed.count() / 1000.f;

		m_client.send_pos_update(m_camera, m_UUID);
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
