#include "vk_utils.h"
#include "SDL_video.h"
#include "SDL_vulkan.h"
#include "vk_types.h"
#include <iostream>
#include <vulkan/vulkan_core.h>

VkResult Engine::create_debug_utils_messenger_EXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger)
{
	auto func = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
	if (func != nullptr) {
		return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
	} else {
		return VK_ERROR_EXTENSION_NOT_PRESENT;
	}
}

void Engine::destroy_debug_utils_messenger_EXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator)
{
	auto func = (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkDestroyDebugMessengerEXT");
	if (func != nullptr) {
		func(instance, debugMessenger, pAllocator);
	}
}

std::vector<const char*> Engine::get_required_extensions(SDL_Window* window) {
	uint32_t sdlExtensionCount = 0;
	if (SDL_Vulkan_GetInstanceExtensions(window, &sdlExtensionCount, nullptr) == SDL_FALSE) {
		std::cerr << "Failed to get the number of extensions." << std::endl;
		abort();
	}

	std::vector<const char*> sdlExtensions(sdlExtensionCount);
	if(SDL_Vulkan_GetInstanceExtensions(window, &sdlExtensionCount, sdlExtensions.data()) == SDL_FALSE) {
		std::cerr << "Failed to get the extensions \n";
		abort();
	}

	std::vector<const char*> extensions(sdlExtensions.data(), sdlExtensions.data() + sdlExtensionCount);

	if (c_enableValidationLayers) {
		extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
	}

	return extensions;
}
