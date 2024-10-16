#pragma once

#include "SDL_video.h"
#include <string>

namespace Engine {

VkResult create_debug_utils_messenger_EXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger);

void destroy_debug_utils_messenger_EXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator);

std::vector<const char*> get_required_extensions(SDL_Window* window);

std::vector<char> read_file(const std::string& filename);

VkShaderModule create_shader_module(VkDevice device, const std::vector<char>& code);
}

