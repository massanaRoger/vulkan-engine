#include "SDL_video.h"
#include <vulkan/vulkan_core.h>

namespace Engine {
class Renderer {
public:
	void init_vulkan(SDL_Window* window);
	void cleanup();
private:
	VkInstance m_instance;
	VkDebugUtilsMessengerEXT m_debugMessenger;

	void create_instance(SDL_Window* window);
	bool check_validation_layer_support();
	void setup_debug_messenger();
	static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
		VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
		VkDebugUtilsMessageTypeFlagsEXT messageType,
		const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
		void* pUserData);
	void populate_debug_messenger_create_info(VkDebugUtilsMessengerCreateInfoEXT& createInfo);
};
}
