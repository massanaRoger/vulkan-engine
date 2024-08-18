#include "vk_renderer.h"
#include "SDL_stdinc.h"
#include "SDL_video.h"
#include "vk_types.h"

#include <cstdint>
#include <iostream>
#include <stdlib.h>
#include <vector>
#include <vulkan/vulkan_core.h>
#include <SDL_vulkan.h>

void Engine::Vulkan::Renderer::init_vulkan(SDL_Window* window)
{
	create_instance(window);
}

void Engine::Vulkan::Renderer::create_instance(SDL_Window* window)
{
	VkApplicationInfo appInfo{};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = "3D Engine";
	appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.pEngineName = "Engine";
	appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.apiVersion = VK_API_VERSION_1_3;

	VkInstanceCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInfo.pApplicationInfo = &appInfo;

	uint32_t extensionCount = 0;
	if (SDL_Vulkan_GetInstanceExtensions(window, &extensionCount, nullptr) == SDL_FALSE) {
		std::cerr << "Failed to get the number of extensions." << std::endl;
		abort();
	}

	std::vector<const char*> sdlExtensions(extensionCount);
	if(SDL_Vulkan_GetInstanceExtensions(window, &extensionCount, sdlExtensions.data()) == SDL_FALSE) {
		std::cerr << "Failed to get the extensions \n";
		abort();
	}

	createInfo.enabledExtensionCount = extensionCount;
	createInfo.ppEnabledExtensionNames = sdlExtensions.data();

	createInfo.enabledLayerCount = 0;

	VK_CHECK(vkCreateInstance(&createInfo, nullptr, &m_instance));
}
