#include "SDL_video.h"
#include <cstdint>
#include <optional>
#include <vulkan/vulkan_core.h>

namespace Engine {

struct QueueFamilyIndices {
	std::optional<uint32_t> graphicsFamily;
};

class Renderer {
public:
	void init_vulkan(SDL_Window* window);
	void cleanup();
private:
	VkInstance m_instance;
	VkDebugUtilsMessengerEXT m_debugMessenger;
	VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;

	void create_instance(SDL_Window* window);
	bool check_validation_layer_support();
	void setup_debug_messenger();
	static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
		VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
		VkDebugUtilsMessageTypeFlagsEXT messageType,
		const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
		void* pUserData);
	void populate_debug_messenger_create_info(VkDebugUtilsMessengerCreateInfoEXT& createInfo);
	void pick_physical_device();
	QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);
};
}
