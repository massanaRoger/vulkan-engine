#pragma once

#include "SDL_video.h"
#include "core/camera.h"
#include "core/scene.h"
#include "imgui.h"
#include "vulkan/vk_asset_loader.h"
#include "vulkan/vk_descriptors.h"
#include "vulkan/vk_pipeline_manager.h"
#include "vulkan/vk_resource_manager.h"
#include "vulkan/vk_swapchain_manager.h"
#include "vulkan/vk_types.h"
#include <cstdint>
#include <memory>
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
	ShadowInstance shadow;
	ShadowInstance shadowcube;
	float alphaCutoff;
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

struct OffscreenSceneData {
	glm::vec4 lightPos;
	float farPlane;
};

struct GPUSceneData {
	glm::mat4 view;
	glm::mat4 proj;
	glm::mat4 viewproj;
	glm::mat4 lightSpaceMatrix;
	glm::vec4 ambientColor;
	glm::vec4 sunlightDirection; // w for sun power
	glm::vec4 sunlightColor;
	glm::vec4 camPos;
	glm::vec4 lightPos[4];
	glm::vec4 lightColors[4];
	uint32_t hasNormalMap;
	float farPlane;
};

struct MeshAsset {
	std::string name;
	std::vector<GeoSurface> surfaces;
	GPUMeshBuffers meshBuffers;
};

struct Vertex {
    glm::vec3 position;
    float uv_x;
    glm::vec3 normal;
    float uv_y;
    glm::vec4 color;
};

class ShadowCube {
public:
	std::string pipelineName;
	VkDescriptorSetLayout shadowLayout;
	VkRenderPass renderPass;
	DescriptorWriter writer;
	AllocatedImage depthImage;
	AllocatedImage cubeMap;

	VkSampler depthSampler;
	VkSampler cubemapSampler;

	VkFramebuffer framebuffers[6];
	VkImageView faceImageViews[6];

	struct ShadowResources {
		AllocatedImage colorImage;
		VkSampler colorSampler;
		VkBuffer dataBuffer;
		uint32_t dataBufferOffset;
	};

	struct ShadowConstants {
		glm::mat4 lightSpaceMatrix;
		glm::mat4 model;
		//padding, we need it anyway for uniform buffers
		glm::vec4 extra[8];
	};

	void build_pipelines(Renderer& renderer);
	void clear_resources(VkDevice device);

	ShadowInstance write_material(VkDevice device, const ShadowResources& resources, DescriptorAllocatorGrowable& descriptorAllocator);
};

class Shadow {
public:
	std::string pipelineName;
	VkDescriptorSetLayout shadowLayout;
	VkRenderPass renderPass;
	DescriptorWriter writer;
	AllocatedImage depthImage;
	VkSampler depthSampler;
	VkFramebuffer framebuffer;

	struct ShadowResources {
		VkBuffer dataBuffer;
		uint32_t dataBufferOffset;
	};

	struct ShadowConstants {
		glm::mat4 lightSpaceMatrix;
		glm::mat4 model;
		//padding, we need it anyway for uniform buffers
		glm::vec4 extra[8];
	};

	void build_pipelines(Renderer& renderer);
	void clear_resources(VkDevice device);

	ShadowInstance write_material(VkDevice device, const ShadowResources& resources, DescriptorAllocatorGrowable& descriptorAllocator);
};

class GLTFMetallic_Roughness {
public:
	std::string opaquePipelineName;
	std::string transparentPipelineName;

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
		AllocatedImage normalImage;
		VkSampler normalSampler;
		AllocatedImage aoImage;
		VkSampler aoSampler;
		AllocatedImage depthImage;
		VkSampler depthSampler;
 		VkBuffer dataBuffer;
		uint32_t dataBufferOffset;
		uint32_t hasNormalMap;
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

struct EngineStats {
    float frameTime;
    int triangleCount;
    int drawcallCount;
    float sceneUpdateTime;
    float meshDrawTime;
};

class Renderer {
public:
	SwapchainManager swapchainManager;
	ResourceManager resourceManager;
	PipelineManager pipelineManager;

	const uint32_t shadowMapize{ 2048 };
	Scene* scene;
	EngineStats stats;
	bool frameBufferResized = false;
	VkDevice device;
	VkRenderPass renderPass;
	VkSampleCountFlagBits msaaSamples = VK_SAMPLE_COUNT_1_BIT;
	AllocatedImage errorCheckerboardImage;

	VkSampler defaultSamplerLinear;
	AllocatedImage whiteImage;

	GLTFMetallic_Roughness metalRoughMaterial;
	// Shadow shadow;
	ShadowCube shadowcube;

	static Renderer& getInstance();

	void init_vulkan(SDL_Window* window, Scene* scene);
	void draw_frame(const Camera& camera, ImDrawData* drawData);
	GPUMeshBuffers upload_mesh(std::vector<uint32_t>& indices, std::vector<Vertex>& vertices);

	[[nodiscard]] VkDescriptorSetLayout get_gpu_scene_data_descriptor_layout() const;
	[[nodiscard]] VkDescriptorSetLayout get_material_descriptor_layout() const;

	void update_loaded_scenes(Scene& scene);

	AllocatedBuffer create_buffer(VkDeviceSize size, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage, VkSharingMode sharingMode);
	AllocatedImage create_image(void* data, VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped);

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

	VkDescriptorSetLayout m_gpuSceneDataDescriptorLayout;
	VkDescriptorSetLayout m_materialDescriptorLayout;
	VkDescriptorSetLayout m_offscreenSceneDataDescriptorLayout;

	std::vector<DescriptorAllocatorGrowable> m_descriptors;
	DescriptorAllocatorGrowable m_globalDescriptorAllocator;
	DescriptorWriter m_descriptorWriter;

	VkDescriptorPool m_imguiPool;

	VkCommandPool m_graphicsCommandPool;
	VkCommandPool m_transferCommandPool;

	uint32_t m_mipLevels;

	MaterialInstance m_defaultData;

	AllocatedBuffer m_gpuSceneDataBuffer;
	AllocatedBuffer m_offscrenSceneDataBuffer;
	AllocatedBuffer m_materialConstants;

	VkSampler m_defaultSamplerNearest;

	std::vector<VkCommandBuffer> m_commandBuffers;
	std::vector<VkSemaphore> m_imageAvailableSemaphores;
	std::vector<VkSemaphore> m_renderFinishedSemaphores;
	std::vector<VkFence> m_inFlightFences;

	uint32_t m_currentFrame;
	GPUSceneData m_sceneData;
	OffscreenSceneData m_offscreenData;

	DrawContext m_mainDrawContext;

	//std::vector<MeshAsset*> m_testMeshes;
	std::unordered_map<entt::entity, std::shared_ptr<LoadedGLTF>> m_loadedScenes;

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

	VkPresentModeKHR choose_swap_present_mode(const std::vector<VkPresentModeKHR>& availablePresentModes);
	VkExtent2D choose_swap_extent(const VkSurfaceCapabilitiesKHR& capabilities);

	void create_image_views();

	void create_descriptor_set_layout();

	void prepare_cube_map();

	void create_render_pass();
	void create_shadow_render_pass();
	void create_shadowcube_render_pass();

	void init_default_data();

	void create_frame_buffers();
	VkCommandBuffer begin_single_time_commands();
	void end_single_time_commands(VkCommandBuffer commandBuffer);

	void create_command_pools();
	void create_color_resources();
	void create_depth_resources();
	VkFormat find_supported_format(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);
	void record_command_buffer(VkCommandBuffer commandBuffer, uint32_t imageIndex, ImDrawData* drawData);

	void update_cube_face(uint32_t faceIndex, VkCommandBuffer commandBuffer, VkDescriptorSet descriptor);

	void create_sync_objects();
	void create_command_buffers();
	void create_descriptor_sets();
	uint32_t find_memory_type(uint32_t typeFilter, VkMemoryPropertyFlags properties);

	VkSampleCountFlagBits get_max_usable_sample_count();

	void cleanup_swapchain();
};
}
