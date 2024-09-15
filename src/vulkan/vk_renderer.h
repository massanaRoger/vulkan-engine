#pragma once

#include "SDL_video.h"
#include "core/camera.h"
#include "imgui.h"
#include "vulkan/vk_asset_loader.h"
#include "vulkan/vk_descriptors.h"
#include "vulkan/vk_types.h"
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>
#include <vulkan/vulkan_core.h>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>

namespace Engine {

class Renderer;
struct LoadedGLTF;

const int MAX_FRAMES_IN_FLIGHT=2;

struct GLTFMaterial {
	MaterialInstance data;
};
struct GeoSurface {
	uint32_t startIndex;
	uint32_t count;
	std::shared_ptr<GLTFMaterial> material;
};


struct GPUMeshBuffers {
    AllocatedBuffer indexBuffer;
    AllocatedBuffer vertexBuffer;
    VkDeviceAddress vertexBufferAddress;
};

struct GPUSceneData {
    glm::mat4 view;
    glm::mat4 proj;
    glm::mat4 viewproj;
    glm::vec4 ambientColor;
    glm::vec4 sunlightDirection; // w for sun power
    glm::vec4 sunlightColor;
};

struct MeshAsset {
	std::string name;
	std::vector<GeoSurface> surfaces;
	GPUMeshBuffers meshBuffers;
};

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
    glm::vec3 position;
    float uv_x;
    glm::vec3 normal;
    float uv_y;
    glm::vec4 color;
};

class GLTFMetallic_Roughness {
public:
	MaterialPipeline opaquePipeline;
	MaterialPipeline transparentPipeline;

	VkDescriptorSetLayout materialLayout;

	struct MaterialConstants {
		glm::vec4 colorFactors;
		glm::vec4 metal_rough_factors;
		//padding, we need it anyway for uniform buffers
		glm::vec4 extra[14];
	};

	struct MaterialResources {
		AllocatedImage colorImage;
		VkSampler colorSampler;
		AllocatedImage metalRoughImage;
		VkSampler metalRoughSampler;
		VkBuffer dataBuffer;
		uint32_t dataBufferOffset;
	};

	DescriptorWriter writer;

	void build_pipelines(Renderer& renderer);
	void clear_resources(VkDevice device);

	MaterialInstance write_material(VkDevice device, MaterialPass pass, const MaterialResources& resources, DescriptorAllocatorGrowable& descriptorAllocator);
};

struct MeshNode : public Node {

	std::shared_ptr<MeshAsset> mesh;

	virtual void draw(const glm::mat4& topMatrix, DrawContext& ctx) override;
};

struct DrawContext {
	std::vector<RenderObject> opaqueSurfaces;
	std::vector<RenderObject> transparentSurfaces;
};


class Renderer {
public:

	bool frameBufferResized = false;
	VkDevice device;
	VkExtent2D swapchainExtent;
	VkRenderPass renderPass;
	VkSampleCountFlagBits msaaSamples = VK_SAMPLE_COUNT_1_BIT;
	AllocatedImage errorCheckerboardImage;

	VkSampler defaultSamplerLinear;
	AllocatedImage whiteImage;

	GLTFMetallic_Roughness metalRoughMaterial;

	static Renderer& getInstance();

	void init_vulkan(SDL_Window* window);
	void draw_frame(const Camera& camera, ImDrawData* drawData);
	GPUMeshBuffers upload_mesh(std::vector<uint32_t>& indices, std::vector<Vertex>& vertices);

	[[nodiscard]] VkDescriptorSetLayout get_gpu_scene_data_descriptor_layout() const;
	[[nodiscard]] VkDescriptorSetLayout get_material_descriptor_layout() const;

	AllocatedBuffer create_buffer(VkDeviceSize size, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage, VkSharingMode sharingMode);
	AllocatedImage create_image(void* data, VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped);

	void destroy_image(const AllocatedImage& img);
	void destroy_buffer(const AllocatedBuffer& buffer);

	void cleanup();

	Renderer(const Renderer&) = delete;
	Renderer& operator=(const Renderer&) = delete;

private:

	Renderer() = default;

	SDL_Window* m_window;
	VkInstance m_instance;
	VkDebugUtilsMessengerEXT m_debugMessenger;
	VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
	VkQueue m_graphicsQueue;
	VkQueue m_presentQueue;
	VkQueue m_transferQueue;
	QueueFamilyIndices m_queueFamilies;

	VkSurfaceKHR m_surface;
	VkSwapchainKHR m_swapchain;
	std::vector<VkImage> m_swapchainImages;
	VkFormat m_swapchainImageFormat;
	std::vector<VkImageView> m_swapchainImageViews;

	VkDescriptorSetLayout m_gpuSceneDataDescriptorLayout;
	VkDescriptorSetLayout m_materialDescriptorLayout;

	std::vector<DescriptorAllocatorGrowable> m_descriptors;
	DescriptorAllocatorGrowable m_globalDescriptorAllocator;
	DescriptorWriter m_descriptorWriter;
	std::vector<VkFramebuffer> m_swapchainFramebuffers;

	VkDescriptorPool m_imguiPool;

	VkCommandPool m_graphicsCommandPool;
	VkCommandPool m_transferCommandPool;

	uint32_t m_mipLevels;
	VmaAllocator m_allocator;

	MaterialInstance m_defaultData;

	AllocatedImage m_drawImage;
	AllocatedImage m_depthImage;

	AllocatedBuffer m_gpuSceneDataBuffer;
	AllocatedBuffer m_materialConstants;

	VkSampler m_defaultSamplerNearest;

	std::vector<VkCommandBuffer> m_commandBuffers;
	std::vector<VkSemaphore> m_imageAvailableSemaphores;
	std::vector<VkSemaphore> m_renderFinishedSemaphores;
	std::vector<VkFence> m_inFlightFences;

	uint32_t m_currentFrame;
	GPUSceneData m_sceneData;

	DrawContext m_mainDrawContext;

	//std::vector<MeshAsset*> m_testMeshes;
	std::unordered_map<std::string, std::shared_ptr<LoadedGLTF>> m_loadedScenes;

	void update_scene(const Camera& camera);

	void create_instance();

	void init_imgui();
	void draw_imgui(VkCommandBuffer cmd, VkImageView targetImageView);
	void cleanup_imgui();

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

	void create_scene_data();

	SwapChainSupportDetails query_swap_chain_support(VkPhysicalDevice device);
	VkSurfaceFormatKHR choose_swap_chain_format(const std::vector<VkSurfaceFormatKHR>& availableFormats);
	VkPresentModeKHR choose_swap_present_mode(const std::vector<VkPresentModeKHR>& availablePresentModes);
	VkExtent2D choose_swap_extent(const VkSurfaceCapabilitiesKHR& capabilities);
	void create_swapchain();
	void recreate_swapchain();
	void cleanup_swapchain();

	void create_image_views();

	void create_descriptor_set_layout();

	void create_render_pass();

	void init_default_data();

	void create_frame_buffers();
	VkCommandBuffer begin_single_time_commands();
	void end_single_time_commands(VkCommandBuffer commandBuffer);

	void create_command_pools();
	void create_color_resources();
	void create_depth_resources();
	VkFormat find_supported_format(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);
	void generate_mipmaps(VkImage image, VkFormat imageFormat, int32_t texWidth, int32_t texHeight, uint32_t mipLevels);
	void create_image(uint32_t width, uint32_t height, uint32_t mipLevels, VkSampleCountFlagBits numSamples, VkFormat format, VkImageTiling tiling, 
		   VkImageUsageFlags usage, VmaMemoryUsage memoryUsage, VkImage& image, VmaAllocation& imageAllocation);
	AllocatedImage create_image(VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped);
	void record_command_buffer(VkCommandBuffer commandBuffer, uint32_t imageIndex, ImDrawData* drawData);
	void transition_image_layout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t mipLevels);
	void copy_buffer_to_image(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);
	void copy_buffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);

	void create_sync_objects();
	void create_command_buffers();
	void create_descriptor_sets();
	uint32_t find_memory_type(uint32_t typeFilter, VkMemoryPropertyFlags properties);

	VkSampleCountFlagBits get_max_usable_sample_count();
	void create_allocator();
};
}
