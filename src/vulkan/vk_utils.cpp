#include "vk_utils.h"
#include "SDL_video.h"
#include "SDL_vulkan.h"
#include "fmt/core.h"
#include "glm/fwd.hpp"
#include "vk_types.h"
#include "fastgltf/util.hpp"
#include "vulkan/vk_renderer.h"
#include <cstddef>
#include <fstream>
#include <ios>
#include <iostream>
#include <ostream>
#include <vulkan/vulkan_core.h>
#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/parser.hpp>
#include <fastgltf/tools.hpp>

namespace Engine {

VkResult create_debug_utils_messenger_EXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger)
{
	auto func = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
	if (func != nullptr) {
		return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
	} else {
		return VK_ERROR_EXTENSION_NOT_PRESENT;
	}
}

void destroy_debug_utils_messenger_EXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator)
{
	auto func = (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
	if (func != nullptr) {
		func(instance, debugMessenger, pAllocator);
	}
}

std::vector<const char*> get_required_extensions(SDL_Window* window) {
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

	#ifdef PLATFORM_NEEDS_PORTABILITY
		extensions.push_back(VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME);
	#endif

	if (c_enableValidationLayers) {
		extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
	}

	return extensions;
}

std::vector<char> read_file(const std::string& filename) {
	std::ifstream file(filename, std::ios::ate | std::ios::binary);

	if (!file.is_open()) {
		std::cerr << "Failed to open file" << std::endl;
		return std::vector<char>();
	}

	size_t fileSize = (size_t) file.tellg();
	std::vector<char> buffer(fileSize);

	file.seekg(0);
	file.read(buffer.data(), fileSize);
	file.close();

	return buffer;
}

VkShaderModule create_shader_module(VkDevice device, const std::vector<char>& code)
{

	VkShaderModuleCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	createInfo.codeSize = code.size();
	createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

	VkShaderModule shaderModule;
	VK_CHECK(vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule));

	return shaderModule;
}

}
