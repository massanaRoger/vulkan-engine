#include "SDL_video.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace Engine {

const int MAX_FRAMES_IN_FLIGHT=2;

struct QueueFamilyIndices {
	std::optional<uint32_t> graphicsFamily;
	std::optional<uint32_t> presentFamily;

	bool isComplete() {
		return graphicsFamily.has_value() && presentFamily.has_value();
	}
};

struct SwapChainSupportDetails {
	VkSurfaceCapabilitiesKHR capabilities;
	std::vector<VkSurfaceFormatKHR> formats;
	std::vector<VkPresentModeKHR> presentModes;
};

struct Vertex {
	glm::vec2 pos;
	glm::vec3 color;

	static VkVertexInputBindingDescription get_binding_description()
	{
		VkVertexInputBindingDescription bindingDescription{};
		bindingDescription.binding = 0;
		bindingDescription.stride = sizeof(Vertex);
		bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

		return bindingDescription;
	}

	static std::array<VkVertexInputAttributeDescription, 2> get_attribute_descriptions()
	{
		std::array<VkVertexInputAttributeDescription, 2> attributeDescriptions{};
		attributeDescriptions[0].binding = 0;
		attributeDescriptions[0].location = 0;
		attributeDescriptions[0].format = VK_FORMAT_R32G32_SFLOAT;
		attributeDescriptions[0].offset = offsetof(Vertex, pos);

		attributeDescriptions[1].binding = 0;
		attributeDescriptions[1].location = 1;
		attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
		attributeDescriptions[1].offset = offsetof(Vertex, color);

		return attributeDescriptions;
	}
};

class Renderer {
public:
	static Renderer& getInstance();

	void init_vulkan(SDL_Window* window);
	void draw_frame();
	void cleanup();

	Renderer(const Renderer&) = delete;
	Renderer& operator=(const Renderer&) = delete;

	bool frameBufferResized = false;
private:
	Renderer() = default;

	SDL_Window* m_window;
	VkInstance m_instance;
	VkDebugUtilsMessengerEXT m_debugMessenger;
	VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
	VkDevice m_device;
	VkQueue m_graphicsQueue;
	VkQueue m_presentQueue;
	VkSurfaceKHR m_surface;
	VkSwapchainKHR m_swapchain;
	std::vector<VkImage> m_swapchainImages;
	VkFormat m_swapchainImageFormat;
	VkExtent2D m_swapchainExtent;
	std::vector<VkImageView> m_swapchainImageViews;
	VkRenderPass m_renderPass;
	VkPipelineLayout m_pipelineLayout;
	VkPipeline m_graphicsPipeline;
	std::vector<VkFramebuffer> m_swapchainFramebuffers;
	VkCommandPool m_commandPool;
	VkBuffer m_vertexBuffer;
	VkDeviceMemory m_vertexBufferMemory;
	std::vector<VkCommandBuffer> m_commandBuffers;
	std::vector<VkSemaphore> m_imageAvailableSemaphores;
	std::vector<VkSemaphore> m_renderFinishedSemaphores;
	std::vector<VkFence> m_inFlightFences;

	uint32_t m_currentFrame;

	std::vector<Vertex> vertices = {
		{{0.0f, -0.5f}, {1.0f, 1.0f, 1.0f}},
		{{0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}},
		{{-0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}}
	};

	void create_instance();

	bool check_validation_layer_support();
	void setup_debug_messenger();
	static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
		VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
		VkDebugUtilsMessageTypeFlagsEXT messageType,
		const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
		void* pUserData);
	void populate_debug_messenger_create_info(VkDebugUtilsMessengerCreateInfoEXT& createInfo);

	void pick_physical_device();
	void create_logical_device();

	QueueFamilyIndices find_queue_families(VkPhysicalDevice device);

	void create_surface();

	bool check_device_extension_support(VkPhysicalDevice device);

	SwapChainSupportDetails query_swap_chain_support(VkPhysicalDevice device);
	VkSurfaceFormatKHR choose_swap_chain_format(const std::vector<VkSurfaceFormatKHR>& availableFormats);
	VkPresentModeKHR choose_swap_present_mode(const std::vector<VkPresentModeKHR>& availablePresentModes);
	VkExtent2D choose_swap_extent(const VkSurfaceCapabilitiesKHR& capabilities);
	void create_swapchain();
	void recreate_swapchain();
	void cleanup_swapchain();

	void create_image_views();

	void create_graphics_pipeline();
	VkShaderModule create_shader_module(const std::vector<char>& code);

	void create_render_pass();

	void create_frame_buffers();

	void create_command_pool();
	void record_command_buffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);

	void create_sync_objects();
	void create_command_buffers();

	void create_vertex_buffer();
	uint32_t find_memory_type(uint32_t typeFilter, VkMemoryPropertyFlags properties);
};
}
