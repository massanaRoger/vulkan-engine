#pragma once

#include "SDL_video.h"
#include "core/camera.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>
#include <functional>
#include <vk_mem_alloc.h>

namespace Engine {

const int MAX_FRAMES_IN_FLIGHT=2;

struct QueueFamilyIndices {
	std::optional<uint32_t> graphicsFamily;
	std::optional<uint32_t> presentFamily;
	std::optional<uint32_t> transferFamily;

	bool isComplete() {
		return graphicsFamily.has_value() && presentFamily.has_value() && transferFamily.has_value();
	}
};

struct SwapChainSupportDetails {
	VkSurfaceCapabilitiesKHR capabilities;
	std::vector<VkSurfaceFormatKHR> formats;
	std::vector<VkPresentModeKHR> presentModes;
};

struct UniformBufferObject {
	glm::mat4 model;
	glm::mat4 view;
	glm::mat4 proj;
};

struct Vertex {
	glm::vec3 pos;
	glm::vec3 color;
	glm::vec2 texCoord;

	static VkVertexInputBindingDescription get_binding_description()
	{
		VkVertexInputBindingDescription bindingDescription{};
		bindingDescription.binding = 0;
		bindingDescription.stride = sizeof(Vertex);
		bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

		return bindingDescription;
	}

	static std::array<VkVertexInputAttributeDescription, 3> get_attribute_descriptions()
	{
		std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions{};
		attributeDescriptions[0].binding = 0;
		attributeDescriptions[0].location = 0;
		attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
		attributeDescriptions[0].offset = offsetof(Vertex, pos);

		attributeDescriptions[1].binding = 0;
		attributeDescriptions[1].location = 1;
		attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
		attributeDescriptions[1].offset = offsetof(Vertex, color);

		attributeDescriptions[2].binding = 0;
		attributeDescriptions[2].location = 2;
		attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
		attributeDescriptions[2].offset = offsetof(Vertex, texCoord);

		return attributeDescriptions;
	}

	bool operator==(const Vertex& other) const {
		return pos == other.pos && color == other.color && texCoord == other.texCoord;
	}
};

class Renderer {
public:
	static Renderer& getInstance();

	void init_vulkan(SDL_Window* window);
	void draw_frame(const Camera& camera);
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
	VkQueue m_transferQueue;
	QueueFamilyIndices m_queueFamilies;
	VkSurfaceKHR m_surface;
	VkSwapchainKHR m_swapchain;
	std::vector<VkImage> m_swapchainImages;
	VkFormat m_swapchainImageFormat;
	VkExtent2D m_swapchainExtent;
	std::vector<VkImageView> m_swapchainImageViews;
	VkRenderPass m_renderPass;
	VkDescriptorSetLayout m_descriptorSetLayout;
	VkDescriptorPool m_descriptorPool;
	std::vector<VkDescriptorSet> m_descriptorSets;
	VkPipelineLayout m_pipelineLayout;
	VkPipeline m_graphicsPipeline;
	std::vector<VkFramebuffer> m_swapchainFramebuffers;
	VkCommandPool m_graphicsCommandPool;
	VkCommandPool m_transferCommandPool;
	VkBuffer m_vertexBuffer;
	VmaAllocation m_vertexBufferMemory;
	VkBuffer m_indexBuffer;
	VmaAllocation m_indexBufferMemory;
	VkImage m_textureImage;
	VmaAllocation m_textureImageMemory;
	VkImageView m_textureImageView;
	VkSampler m_textureSampler;
	uint32_t m_mipLevels;
	VkImage m_depthImage;
	VmaAllocation m_depthImageMemory;
	VkImageView m_depthImageView;
	VkSampleCountFlagBits m_msaaSamples = VK_SAMPLE_COUNT_1_BIT;
	VkImage m_colorImage;
	VmaAllocation  m_colorImageMemory;
	VkImageView m_colorImageView;
	VmaAllocator m_allocator;

	std::vector<VkBuffer> m_uniformBuffers;
	std::vector<VmaAllocation> m_uniformBuffersMemory;
	std::vector<void*> m_uniformBuffersMapped;

	std::vector<VkCommandBuffer> m_commandBuffers;
	std::vector<VkSemaphore> m_imageAvailableSemaphores;
	std::vector<VkSemaphore> m_renderFinishedSemaphores;
	std::vector<VkFence> m_inFlightFences;

	uint32_t m_currentFrame;

	std::vector<Vertex> m_vertices;
	std::vector<uint32_t> m_indices;

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

	void create_descriptor_set_layout();

	void create_graphics_pipeline();
	VkShaderModule create_shader_module(const std::vector<char>& code);

	void create_render_pass();

	void create_frame_buffers();
	VkCommandBuffer begin_single_time_commands();
	void end_single_time_commands(VkCommandBuffer commandBuffer);
	void create_uniform_buffers();
	void update_uniform_buffer(uint32_t currentImage, const Camera& camera);

	void create_command_pools();
	void create_color_resources();
	void create_depth_resources();
	VkFormat find_supported_format(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);
	void create_texture_image();
	void generate_mipmaps(VkImage image, VkFormat imageFormat, int32_t texWidth, int32_t texHeight, uint32_t mipLevels);
	void create_texture_image_view();
	void create_texture_sampler();
	void create_image(uint32_t width, uint32_t height, uint32_t mipLevels, VkSampleCountFlagBits numSamples, VkFormat format, VkImageTiling tiling, 
				     VkImageUsageFlags usage, VmaMemoryUsage memoryUsage, VkImage& image, VmaAllocation& imageAllocation);
	void record_command_buffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);
	void transition_image_layout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t mipLevels);
	void copy_buffer_to_image(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);
	void create_buffer(VkDeviceSize size, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage, VkSharingMode sharingMode, VkBuffer &buffer, VmaAllocation &bufferMemory);
	void copy_buffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);

	void create_sync_objects();
	void create_command_buffers();
	void create_descriptor_pool();
	void create_descriptor_sets();
	void load_model();
	void create_vertex_buffer();
	void create_index_buffer();
	uint32_t find_memory_type(uint32_t typeFilter, VkMemoryPropertyFlags properties);

	VkSampleCountFlagBits get_max_usable_sample_count();
	void create_allocator();
};
}

namespace std {
    template<> struct hash<Engine::Vertex> {
        size_t operator()(Engine::Vertex const& vertex) const {
            return ((hash<glm::vec3>()(vertex.pos) ^
                    (hash<glm::vec3>()(vertex.color) << 1)) >> 1) ^
                   (hash<glm::vec2>()(vertex.texCoord) << 1);
        }
    };
}
