#include "vk_renderer.h"
#include "SDL_stdinc.h"
#include "SDL_video.h"
#include "core/camera.h"
#include "core/components.h"
#include "core/scene.h"
#include "glm/ext/matrix_float4x4.hpp"
#include "glm/ext/vector_float4.hpp"
#include "glm/geometric.hpp"
#include "vk_types.h"
#include "vk_utils.h"
#include "vulkan/vk_descriptors.h"
#include "imgui.h"
#include "imgui_impl_vulkan.h"
#include "imgui_impl_sdl2.h"
#include "vulkan/vk_swapchain_manager.h"
#include <array>
#include <unordered_map>
#include <vulkan/vulkan_core.h>

#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"
#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <set>
#include <stdlib.h>
#include <vector>
#include <SDL_vulkan.h>

namespace Engine {

glm::mat4 lightViewMatrix = glm::lookAt(glm::vec3(-2.0f, 4.0f, -1.0f), 
                                  glm::vec3( 0.0f, 0.0f,  0.0f), 
                                  glm::vec3( 0.0f, 1.0f,  0.0f));  

Renderer& Renderer::getInstance()
{
	static Renderer instance;
	return instance;
}

void Renderer::init_vulkan(SDL_Window* window, Scene* sc)
{
	m_window = window;
	scene = sc;
	create_instance();
	setup_debug_messenger();
	create_surface();
	pick_physical_device();
	create_logical_device();
	resourceManager.create_allocator(device, m_physicalDevice, m_instance);
	swapchainManager.create_swapchain(device, m_surface, m_physicalDevice, find_queue_families(m_physicalDevice), m_window);
	create_image_views();
	// create_shadow_render_pass();
	create_shadowcube_render_pass();
	create_render_pass();
	init_imgui();
	create_descriptor_set_layout();
	metalRoughMaterial.build_pipelines(getInstance());
	shadowcube.build_pipelines(getInstance());
	create_command_pools();
	create_color_resources();
	create_depth_resources();
	prepare_cube_map();
	create_frame_buffers();
	create_descriptor_sets();
	create_scene_data();
	create_command_buffers();
	create_sync_objects();
	init_default_data();
}

void Renderer::init_default_data()
{
	//3 default textures, white, grey, black. 1 pixel each
	uint32_t white = glm::packUnorm4x8(glm::vec4(1, 1, 1, 1));

	std::array<uint32_t, 16 *16 > pixels; //for 16x16 checkerboard texture
	
	uint32_t black = glm::packUnorm4x8(glm::vec4(0, 0, 0, 0));

	//checkerboard image
	uint32_t magenta = glm::packUnorm4x8(glm::vec4(1, 0, 1, 1));

	for (int x = 0; x < 16; x++) {
		for (int y = 0; y < 16; y++) {
			pixels[y*16 + x] = ((x % 2) ^ (y % 2)) ? magenta : black;
		}
	}

	whiteImage = resourceManager.create_image(device, m_graphicsCommandPool, m_graphicsQueue, m_physicalDevice, (void*)&white, VkExtent3D{ 1, 1, 1 }, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT, false, m_queueFamilies);

	errorCheckerboardImage = resourceManager.create_image(device, m_graphicsCommandPool, m_graphicsQueue, m_physicalDevice, pixels.data(), VkExtent3D{ 16, 16, 1 }, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT, false, m_queueFamilies);

	VkSamplerCreateInfo sampl = {.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};

	sampl.magFilter = VK_FILTER_NEAREST;
	sampl.minFilter = VK_FILTER_NEAREST;

	vkCreateSampler(device, &sampl, nullptr, &m_defaultSamplerNearest);

	sampl.magFilter = VK_FILTER_LINEAR;
	sampl.minFilter = VK_FILTER_LINEAR;
	vkCreateSampler(device, &sampl, nullptr, &defaultSamplerLinear);

	GLTFMetallic_Roughness::MaterialResources materialResources;
	//default the material textures
	materialResources.colorImage = whiteImage;
	materialResources.colorSampler = defaultSamplerLinear;
	materialResources.metalRoughImage = whiteImage;
	materialResources.metalRoughSampler = defaultSamplerLinear;
	materialResources.normalImage = whiteImage;
	materialResources.normalSampler = defaultSamplerLinear;
	materialResources.aoImage = whiteImage;
	materialResources.aoSampler = defaultSamplerLinear;
	materialResources.depthImage = shadowcube.depthImage;
	materialResources.depthSampler = shadowcube.depthSampler;

	//set the uniform buffer for the material data
	m_materialConstants = resourceManager.create_buffer(sizeof(GLTFMetallic_Roughness::MaterialConstants), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, VK_SHARING_MODE_EXCLUSIVE, m_queueFamilies);

	//write the buffer
	void* mappedData;
	VK_CHECK(vmaMapMemory(resourceManager.get_allocator(), m_materialConstants.allocation, &mappedData));
	GLTFMetallic_Roughness::MaterialConstants* sceneUniformData = (GLTFMetallic_Roughness::MaterialConstants*)mappedData;

	sceneUniformData->colorFactors = glm::vec4{ 1, 1, 1, 1 };
	sceneUniformData->metal_rough_factors = glm::vec4{ 1, 0.5, 0, 0 };

	vmaUnmapMemory(resourceManager.get_allocator(), m_materialConstants.allocation);

	materialResources.dataBuffer = m_materialConstants.buffer;
	materialResources.dataBufferOffset = 0;

	std::vector<DescriptorAllocatorGrowable::PoolSizeRatio> sizes =
	{
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 }
	};

	m_globalDescriptorAllocator.init(device, 10, sizes);

	m_defaultData = metalRoughMaterial.write_material(device, MaterialPass::MainColor, materialResources, m_globalDescriptorAllocator);

}

void Renderer::create_instance()
{
	if (c_enableValidationLayers && !check_validation_layer_support()) {
		std::cerr << "Validation layers requested, but not available" << std::endl;
		abort();
	}

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

	auto extensions = get_required_extensions(m_window);
	createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
	createInfo.ppEnabledExtensionNames = extensions.data();

	VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
	if (c_enableValidationLayers) {
		createInfo.enabledLayerCount = static_cast<uint32_t>(c_validationLayers.size());
		createInfo.ppEnabledLayerNames = c_validationLayers.data();

		populate_debug_messenger_create_info(debugCreateInfo);
		createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*) &debugCreateInfo;
	} else {
		createInfo.enabledLayerCount = 0;
		createInfo.pNext = nullptr;
	}

	VK_CHECK(vkCreateInstance(&createInfo, nullptr, &m_instance));
}

void Renderer::init_imgui()
{
	//1: create descriptor pool for IMGUI
	// the size of the pool is very oversize, but it's copied from imgui demo itself.
	VkDescriptorPoolSize pool_sizes[] =
	{
		{ VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
		{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
		{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
	};

	VkDescriptorPoolCreateInfo pool_info = {};
	pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
	pool_info.maxSets = 1000;
	pool_info.poolSizeCount = std::size(pool_sizes);
	pool_info.pPoolSizes = pool_sizes;

	VK_CHECK(vkCreateDescriptorPool(device, &pool_info, nullptr, &m_imguiPool));

	// 2: initialize imgui library

	//this initializes the core structures of imgui
	ImGui::CreateContext();

	//this initializes imgui for SDL
	ImGui_ImplSDL2_InitForVulkan(m_window);

	//this initializes imgui for Vulkan
	ImGui_ImplVulkan_InitInfo init_info = {};
	init_info.Instance = m_instance;
	init_info.PhysicalDevice = m_physicalDevice;
	init_info.Device = device;
	init_info.Queue = m_graphicsQueue;
	init_info.DescriptorPool = m_imguiPool;
	init_info.MinImageCount = 3;
	init_info.ImageCount = 3;
	init_info.MSAASamples = msaaSamples;
	init_info.RenderPass = renderPass;

	ImGui_ImplVulkan_Init(&init_info);

	ImGui_ImplVulkan_CreateFontsTexture();
}

void Renderer::cleanup_imgui()
{
	ImGui_ImplVulkan_Shutdown();
	vkDestroyDescriptorPool(device, m_imguiPool, nullptr);
}

void Renderer::cleanup()
{
	vkDeviceWaitIdle(device);
	SDL_DestroyWindow(m_window);

	cleanup_swapchain();
	cleanup_imgui();

	resourceManager.destroy_image(device, whiteImage);
	resourceManager.destroy_image(device, errorCheckerboardImage);

	vkDestroySampler(device, defaultSamplerLinear, nullptr);
	vkDestroySampler(device, m_defaultSamplerNearest, nullptr);

	metalRoughMaterial.clear_resources(device);

	vkDestroyRenderPass(device, renderPass, nullptr);

	/*for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		vmaUnmapMemory(resourceManager.get_allocator(), m_uniformBuffersMemory[i]);
		vmaDestroyBuffer(resourceManager.get_allocator(), m_uniformBuffers[i], m_uniformBuffersMemory[i]);
	}*/

	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		m_descriptors[i].destroy_pools(device);
	}
	m_globalDescriptorAllocator.destroy_pools(device);

	resourceManager.destroy_buffer(m_gpuSceneDataBuffer);
	resourceManager.destroy_buffer(m_materialConstants);

	vkDestroyDescriptorSetLayout(device, m_gpuSceneDataDescriptorLayout, nullptr);
	vkDestroyDescriptorSetLayout(device, m_materialDescriptorLayout, nullptr);

	vmaDestroyAllocator(resourceManager.get_allocator());

	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		vkDestroySemaphore(device, m_renderFinishedSemaphores[i], nullptr);
		vkDestroySemaphore(device, m_imageAvailableSemaphores[i], nullptr);
		vkDestroyFence(device, m_inFlightFences[i], nullptr);
	}

	vkDestroyCommandPool(device, m_transferCommandPool, nullptr);
	vkDestroyCommandPool(device, m_graphicsCommandPool, nullptr);


	vkDestroyDevice(device, nullptr);

	if (c_enableValidationLayers) {
		destroy_debug_utils_messenger_EXT(m_instance, m_debugMessenger, nullptr);
	}

	vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
	vkDestroyInstance(m_instance, nullptr);
}

void Renderer::cleanup_swapchain()
{

	m_loadedScenes.clear();
	swapchainManager.cleanup_swapchain(device, resourceManager.get_allocator());
}

void Renderer::draw_frame(const Camera& camera, ImDrawData* drawData)
{
	update_scene(camera);

	vkWaitForFences(device, 1, &m_inFlightFences[m_currentFrame], VK_TRUE, UINT64_MAX);

	m_descriptors[m_currentFrame].clear_pools(device);

	uint32_t imageIndex;
	VkResult result = vkAcquireNextImageKHR(device, swapchainManager.get_swapchain(), UINT64_MAX, m_imageAvailableSemaphores[m_currentFrame], VK_NULL_HANDLE, &imageIndex);
	if (result == VK_ERROR_OUT_OF_DATE_KHR) {
		VkFormat depthFormat = find_supported_format(
			{VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
			VK_IMAGE_TILING_OPTIMAL,
			VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
		);

		swapchainManager.recreate_swapchain(device, m_window, m_surface, m_physicalDevice, find_queue_families(m_physicalDevice), renderPass, msaaSamples, msaaSamples, depthFormat, resourceManager.get_allocator());
		return;
	} else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
		std::cerr << "Failed to acquire swapchain image" << std::endl;
		abort();
	}

	vkResetFences(device, 1, &m_inFlightFences[m_currentFrame]);

	vkResetCommandBuffer(m_commandBuffers[m_currentFrame], 0);
	record_command_buffer(m_commandBuffers[m_currentFrame], imageIndex, drawData);

	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

	VkSemaphore waitSemaphores[] = { m_imageAvailableSemaphores[m_currentFrame] };
	VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = waitSemaphores;
	submitInfo.pWaitDstStageMask = waitStages;

	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &m_commandBuffers[m_currentFrame];

	VkSemaphore signalSemaphores[] = {m_renderFinishedSemaphores[m_currentFrame]};
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = signalSemaphores;

	VK_CHECK(vkQueueSubmit(m_graphicsQueue, 1, &submitInfo, m_inFlightFences[m_currentFrame]));

	VkPresentInfoKHR presentInfo{};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = signalSemaphores;

	VkSwapchainKHR swapchains[] = { swapchainManager.get_swapchain() };
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = swapchains;
	presentInfo.pImageIndices = &imageIndex;
	presentInfo.pResults = nullptr;

	result = vkQueuePresentKHR(m_presentQueue, &presentInfo);

	if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || frameBufferResized) {
		frameBufferResized = false;
		VkFormat depthFormat = find_supported_format(
			{VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
			VK_IMAGE_TILING_OPTIMAL,
			VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
		);

		swapchainManager.recreate_swapchain(device, m_window, m_surface, m_physicalDevice, find_queue_families(m_physicalDevice), renderPass, msaaSamples, msaaSamples, depthFormat, resourceManager.get_allocator());

	} else if (result != VK_SUCCESS) {
		std::cerr << "Failed to present swapchain!" << std::endl;
		abort();
	}

	m_currentFrame = (m_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;

}

bool Renderer::check_validation_layer_support()
{
	uint32_t layerCount;
	vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

	std::vector<VkLayerProperties> availableLayers(layerCount);
	vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

	for (const char* layerName : c_validationLayers) {
		bool layerFound = false;

		for (const auto& layerProperties : availableLayers) {
			if (strcmp(layerName, layerProperties.layerName) == 0) {
				layerFound = true;
				break;
			}
		}

		if (!layerFound) {
			return false;
		}
	}

	return true;
}

void Renderer::setup_debug_messenger() {
	if (!c_enableValidationLayers) {
		return;
	}

	VkDebugUtilsMessengerCreateInfoEXT createInfo;
	populate_debug_messenger_create_info(createInfo);

	if (create_debug_utils_messenger_EXT(m_instance, &createInfo, nullptr, &m_debugMessenger) != VK_SUCCESS) {
		std::cerr << "Failed to setup debug messenger" << std::endl;
		abort();
	}
}

VKAPI_ATTR VkBool32 VKAPI_CALL Renderer::debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData) {

    std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;

    return VK_FALSE;
}

void Renderer::populate_debug_messenger_create_info(VkDebugUtilsMessengerCreateInfoEXT& createInfo) {
    createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    createInfo.pfnUserCallback = debugCallback;
}

void Renderer::pick_physical_device()
{
	uint32_t deviceCount = 0;
	vkEnumeratePhysicalDevices(m_instance, &deviceCount, nullptr);
	if (deviceCount == 0) {
		std::cerr << "No suitable devices found" << std::endl;
		abort();
	}

	std::vector<VkPhysicalDevice> devices(deviceCount);
	vkEnumeratePhysicalDevices(m_instance, &deviceCount, devices.data());

	// Check if device is suitable
	for (const auto& device : devices) {
		VkPhysicalDeviceProperties2 deviceProperties{};
		deviceProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;

		VkPhysicalDeviceBufferDeviceAddressFeaturesEXT bufferDeviceAddressFeaturesEXT{};
		bufferDeviceAddressFeaturesEXT.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES_EXT;
		bufferDeviceAddressFeaturesEXT.pNext = nullptr;

		VkPhysicalDeviceFeatures2 deviceFeatures{};
		deviceFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
		deviceFeatures.pNext = &bufferDeviceAddressFeaturesEXT;

		vkGetPhysicalDeviceProperties2(device, &deviceProperties);
		vkGetPhysicalDeviceFeatures2(device, &deviceFeatures);

		// Optional will have value if at least it has one queue family for displaying graphics
		QueueFamilyIndices indices = find_queue_families(device);

		// If has vulkan support and geometry shader and has a graphics queue family
		if (deviceProperties.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU
			&& deviceFeatures.features.geometryShader
			&& deviceFeatures.features.samplerAnisotropy
			&& indices.isComplete()
			// Check if device supports the required extensions
			&& check_device_extension_support(device)
		) {
			// Check swapchain support
			SwapchainSupportDetails swapChainSupport = swapchainManager.query_swapchain_support(device, m_surface);
			if (!swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty()) {
				m_physicalDevice = device;
				msaaSamples = get_max_usable_sample_count();
				break;
			}
		}
	}

	if (m_physicalDevice == VK_NULL_HANDLE) {
		std::cerr << "No suitable GPU found" << std::endl;
		abort();
	}
}

QueueFamilyIndices Renderer::find_queue_families(VkPhysicalDevice device)
{
	QueueFamilyIndices indices;
	
	uint32_t queueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties2(device, &queueFamilyCount, nullptr);

	std::vector<VkQueueFamilyProperties2> queueFamilies(queueFamilyCount);
	for (auto& queueFamily : queueFamilies) {
		queueFamily.sType = VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2;
	}
	vkGetPhysicalDeviceQueueFamilyProperties2(device, &queueFamilyCount, queueFamilies.data());

	int i = 0;
	for (const auto& queueFamily : queueFamilies) {
		
		if (queueFamily.queueFamilyProperties.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
			// If it has a dedicated graphics family
		
			indices.graphicsFamily = i;
		} else if (queueFamily.queueFamilyProperties.queueFlags & VK_QUEUE_TRANSFER_BIT) {
			// Dedicated transfer family without graphics
		
			indices.transferFamily = i;
		}

		VkBool32 presentSupport = false;
		vkGetPhysicalDeviceSurfaceSupportKHR(device, i, m_surface, &presentSupport);
		if (presentSupport) {
			indices.presentFamily = i;
		}

		if (indices.isComplete()) {
			return indices;
		}

		i++;
	}

	std::cerr << "No dedicated transfer queue found" << std::endl;
	abort();
}

void Renderer::create_logical_device()
{
	QueueFamilyIndices indices = find_queue_families(m_physicalDevice);

	std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
	std::set<uint32_t> uniqueQueueFamilies = {indices.graphicsFamily.value(), indices.presentFamily.value(), indices.transferFamily.value()};

	float queuePriority = 1.0f;
	for (uint32_t queueFamily : uniqueQueueFamilies) {
		VkDeviceQueueCreateInfo queueCreateInfo{};
		queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfo.queueFamilyIndex = queueFamily;
		queueCreateInfo.queueCount = 1;
		queueCreateInfo.pQueuePriorities = &queuePriority;
		queueCreateInfos.push_back(queueCreateInfo);
	}

	VkPhysicalDeviceFeatures deviceFeatures{};
	deviceFeatures.samplerAnisotropy = VK_TRUE;

	VkPhysicalDeviceBufferDeviceAddressFeatures bufferDeviceAddressFeatures = {};
	bufferDeviceAddressFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;
	bufferDeviceAddressFeatures.bufferDeviceAddress = VK_TRUE;

	VkPhysicalDeviceVulkan12Features vulkan12Features{};
	vulkan12Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
	vulkan12Features.bufferDeviceAddress = VK_TRUE;  // Enable the bufferDeviceAddress feature
	bufferDeviceAddressFeatures.pNext = &bufferDeviceAddressFeatures;

	VkDeviceCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
	createInfo.pQueueCreateInfos = queueCreateInfos.data();
	createInfo.pEnabledFeatures = &deviceFeatures;
	createInfo.pNext = &vulkan12Features;

	// Enable extensions
	createInfo.enabledExtensionCount = static_cast<uint32_t>(c_deviceExtensions.size());
	createInfo.ppEnabledExtensionNames = c_deviceExtensions.data();

	if (c_enableValidationLayers) {
		createInfo.enabledLayerCount = static_cast<uint32_t>(c_validationLayers.size());
		createInfo.ppEnabledLayerNames = c_validationLayers.data();
	} else {
		createInfo.enabledLayerCount = 0;
	}

	VK_CHECK(vkCreateDevice(m_physicalDevice, &createInfo, nullptr, &device));

	vkGetDeviceQueue(device, indices.graphicsFamily.value(), 0, &m_graphicsQueue);
	vkGetDeviceQueue(device, indices.presentFamily.value(), 0, &m_presentQueue);
	vkGetDeviceQueue(device, indices.transferFamily.value(), 0, &m_transferQueue);
}

void Renderer::create_surface()
{
	if (SDL_Vulkan_CreateSurface(m_window, m_instance, &m_surface) == SDL_FALSE) {
		std::cerr << "Failed to create window surface" << std::endl;
		abort();
	}
}

bool Renderer::check_device_extension_support(VkPhysicalDevice device)
{
	uint32_t extensionCount;
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

	std::vector<VkExtensionProperties> availableExtensions(extensionCount);
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

	std::set<std::string> requiredExtensions(c_deviceExtensions.begin(), c_deviceExtensions.end());

	for (const auto& extension : availableExtensions) {
		requiredExtensions.erase(extension.extensionName);
	}

	return requiredExtensions.empty();
}



void Renderer::create_image_views()
{
	swapchainManager.create_swapchain_image_views(device);
}

void Renderer::create_descriptor_set_layout()
{

	VkDescriptorSetLayoutBinding offscreenSceneDataBinding{};
	offscreenSceneDataBinding.binding = 0;
	offscreenSceneDataBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	offscreenSceneDataBinding.descriptorCount = 1;
	offscreenSceneDataBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
	offscreenSceneDataBinding.pImmutableSamplers = nullptr;

	std::array<VkDescriptorSetLayoutBinding, 1> offscreenBindings = { offscreenSceneDataBinding };
	VkDescriptorSetLayoutCreateInfo offscreenLayoutInfo{};
	offscreenLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	offscreenLayoutInfo.bindingCount = static_cast<uint32_t>(offscreenBindings.size());
	offscreenLayoutInfo.pBindings = offscreenBindings.data();

	VK_CHECK(vkCreateDescriptorSetLayout(device, &offscreenLayoutInfo, nullptr, &m_offscreenSceneDataDescriptorLayout));

	VkDescriptorSetLayoutBinding gpuSceneDataBinding{};
	gpuSceneDataBinding.binding = 0;
	gpuSceneDataBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	gpuSceneDataBinding.descriptorCount = 1;
	gpuSceneDataBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
	gpuSceneDataBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutBinding materialDataBinding{};
	materialDataBinding.binding = 0;
	materialDataBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	materialDataBinding.descriptorCount = 1;
	materialDataBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	VkDescriptorSetLayoutBinding colorTexBinding{};
	colorTexBinding.binding = 1;
	colorTexBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	colorTexBinding.descriptorCount = 1;
	colorTexBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	VkDescriptorSetLayoutBinding metalRoughTexBinding{};
	metalRoughTexBinding.binding = 2;
	metalRoughTexBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	metalRoughTexBinding.descriptorCount = 1;
	metalRoughTexBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	std::array<VkDescriptorSetLayoutBinding, 1> sceneBindings = { gpuSceneDataBinding };
	VkDescriptorSetLayoutCreateInfo sceneLayoutInfo{};
	sceneLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	sceneLayoutInfo.bindingCount = static_cast<uint32_t>(sceneBindings.size());
	sceneLayoutInfo.pBindings = sceneBindings.data();

	VK_CHECK(vkCreateDescriptorSetLayout(device, &sceneLayoutInfo, nullptr, &m_gpuSceneDataDescriptorLayout));

	std::array<VkDescriptorSetLayoutBinding, 3> materialBindings = { materialDataBinding, colorTexBinding, metalRoughTexBinding };
	VkDescriptorSetLayoutCreateInfo materialLayoutInfo{};
	materialLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	materialLayoutInfo.bindingCount = static_cast<uint32_t>(materialBindings.size());
	materialLayoutInfo.pBindings = materialBindings.data();

	VK_CHECK(vkCreateDescriptorSetLayout(device, &materialLayoutInfo, nullptr, &m_materialDescriptorLayout));
}

void Renderer::create_shadowcube_render_pass()
{
	VkFormat supportedFormat = find_supported_format(
		{VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
		VK_IMAGE_TILING_OPTIMAL,
		VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
	);
	VkAttachmentDescription osAttachments[2] = {};

	osAttachments[0].format = VK_FORMAT_R32_SFLOAT;
	osAttachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
	osAttachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	osAttachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	osAttachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	osAttachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	osAttachments[0].initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	osAttachments[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	// Depth attachment
	osAttachments[1].format = supportedFormat;
	osAttachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
	osAttachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	osAttachments[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	osAttachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	osAttachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	osAttachments[1].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	osAttachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkAttachmentReference colorReference = {};
	colorReference.attachment = 0;
	colorReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentReference depthReference = {};
	depthReference.attachment = 1;
	depthReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass = {};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorReference;
	subpass.pDepthStencilAttachment = &depthReference;

	VkRenderPassCreateInfo renderPassCreateInfo{};
	renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassCreateInfo.attachmentCount = 2;
	renderPassCreateInfo.pAttachments = osAttachments;
	renderPassCreateInfo.subpassCount = 1;
	renderPassCreateInfo.pSubpasses = &subpass;

	VK_CHECK(vkCreateRenderPass(device, &renderPassCreateInfo, nullptr, &shadowcube.renderPass));
}

void Renderer::create_shadow_render_pass()
{
	VkAttachmentDescription2 attachmentDescription{};
	attachmentDescription.sType = VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2;
	attachmentDescription.format = find_supported_format(
		{VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
		VK_IMAGE_TILING_OPTIMAL,
		VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
	);
	attachmentDescription.samples = VK_SAMPLE_COUNT_1_BIT;
	attachmentDescription.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachmentDescription.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachmentDescription.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachmentDescription.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachmentDescription.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachmentDescription.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

	VkAttachmentReference2 depthAttachmentRef{};
	depthAttachmentRef.sType = VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2;
	depthAttachmentRef.attachment = 0;
	depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkSubpassDescription2 subpass{};
	subpass.sType = VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_2;
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 0;
	subpass.pDepthStencilAttachment = &depthAttachmentRef;

	std::array<VkSubpassDependency2, 2> dependencies{};

	dependencies[0].sType = VK_STRUCTURE_TYPE_SUBPASS_DEPENDENCY_2;
	dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
	dependencies[0].dstSubpass = 0;
	dependencies[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	dependencies[0].dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	dependencies[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
	dependencies[0].dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

	dependencies[1].sType = VK_STRUCTURE_TYPE_SUBPASS_DEPENDENCY_2;
	dependencies[1].srcSubpass = 0;
	dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
	dependencies[1].srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
	dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	dependencies[1].srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	dependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

	VkRenderPassCreateInfo2 renderPassInfo{};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO_2;
	renderPassInfo.attachmentCount = 1;
	renderPassInfo.pAttachments = &attachmentDescription;
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpass;
	renderPassInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
	renderPassInfo.pDependencies = dependencies.data();

//	VK_CHECK(vkCreateRenderPass2(device, &renderPassInfo, nullptr, &shadow.renderPass));

}

void Renderer::create_render_pass()
{
	VkAttachmentDescription2 colorAttachment{};
	colorAttachment.sType = VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2;
	colorAttachment.format = swapchainManager.get_swapchain_image_format();
	colorAttachment.samples = msaaSamples;
	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	colorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentReference2 colorAttachmentRef{};
	colorAttachmentRef.sType = VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2;
	colorAttachmentRef.attachment = 0;
	colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;


	VkAttachmentDescription2 depthAttachment{};
	depthAttachment.sType = VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2;
	depthAttachment.format = find_supported_format(
		{VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
		VK_IMAGE_TILING_OPTIMAL,
		VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
	);
	depthAttachment.samples = msaaSamples;
	depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkAttachmentReference2 depthAttachmentRef{};
	depthAttachmentRef.sType = VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2;
	depthAttachmentRef.attachment = 1;
	depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkAttachmentDescription2 colorAttachmentResolve{};
	colorAttachmentResolve.sType = VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2;
	colorAttachmentResolve.format = swapchainManager.get_swapchain_image_format();
	colorAttachmentResolve.samples = VK_SAMPLE_COUNT_1_BIT;
	colorAttachmentResolve.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachmentResolve.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	colorAttachmentResolve.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachmentResolve.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	colorAttachmentResolve.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	colorAttachmentResolve.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VkAttachmentReference2 colorAttachmentResolveRef{};
	colorAttachmentResolveRef.sType = VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2;
	colorAttachmentResolveRef.attachment = 2;
	colorAttachmentResolveRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkSubpassDescription2 subpass{};
	subpass.sType = VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_2;
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorAttachmentRef;
	subpass.pDepthStencilAttachment = &depthAttachmentRef;
	subpass.pResolveAttachments = &colorAttachmentResolveRef;

	VkSubpassDependency2 dependency{};
	dependency.sType = VK_STRUCTURE_TYPE_SUBPASS_DEPENDENCY_2;
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;
	dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	dependency.srcAccessMask = 0;
	dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

	std::array<VkAttachmentDescription2, 3> attachments = { colorAttachment, depthAttachment, colorAttachmentResolve };
	VkRenderPassCreateInfo2 renderPassInfo{};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO_2;
	renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
	renderPassInfo.pAttachments = attachments.data();
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpass;
	renderPassInfo.dependencyCount = 1;
	renderPassInfo.pDependencies = &dependency;

	VK_CHECK(vkCreateRenderPass2(device, &renderPassInfo, nullptr, &renderPass));
}

void Renderer::create_frame_buffers()
{
	/*
	// Shadowmap attachments
	std::array<VkImageView, 1> shadowmapAttachments = {
		shadow.depthImage.imageView
	};

	VkFramebufferCreateInfo shadowFramebufferInfo = {};
	shadowFramebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	shadowFramebufferInfo.renderPass = shadow.renderPass;
	shadowFramebufferInfo.attachmentCount = static_cast<uint32_t>(shadowmapAttachments.size());
	shadowFramebufferInfo.pAttachments = shadowmapAttachments.data();
	shadowFramebufferInfo.width = shadowMapize;
	shadowFramebufferInfo.height = shadowMapize;
	shadowFramebufferInfo.layers = 1;

	VK_CHECK(vkCreateFramebuffer(device, &shadowFramebufferInfo, nullptr, &shadowcube.framebuffer));
	*/

	// Create depth cubemap framebuffers
	VkImageView cubemapAttachments[2];
	cubemapAttachments[1] = shadowcube.depthImage.imageView;

	VkFramebufferCreateInfo framebufferInfo = {};
	framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	framebufferInfo.renderPass = shadowcube.renderPass;
	framebufferInfo.attachmentCount = 2;
	framebufferInfo.pAttachments = cubemapAttachments;
	framebufferInfo.width = shadowMapize;
	framebufferInfo.height = shadowMapize;
	framebufferInfo.layers = 1;

	for (int i = 0; i < 6; i++) {
		cubemapAttachments[0] = shadowcube.faceImageViews[i];
		VK_CHECK(vkCreateFramebuffer(device, &framebufferInfo, nullptr, &shadowcube.framebuffers[i]));
	}


	swapchainManager.create_swapchain_frame_buffers(device, renderPass);
}

void Renderer::create_command_pools()
{
	m_queueFamilies = find_queue_families(m_physicalDevice);

	VkCommandPoolCreateInfo graphicsPoolInfo{};
	graphicsPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	graphicsPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	graphicsPoolInfo.queueFamilyIndex = m_queueFamilies.graphicsFamily.value();

	VK_CHECK(vkCreateCommandPool(device, &graphicsPoolInfo, nullptr, &m_graphicsCommandPool));

	VkCommandPoolCreateInfo transferPoolInfo{};
	transferPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	transferPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	transferPoolInfo.queueFamilyIndex = m_queueFamilies.transferFamily.value();

	VK_CHECK(vkCreateCommandPool(device, &transferPoolInfo, nullptr, &m_transferCommandPool));
}

void Renderer::prepare_cube_map()
{

	VkFormat format = VK_FORMAT_R32_SFLOAT;
	// Cube map image description
	resourceManager.create_image(shadowMapize, shadowMapize, 1, VK_SAMPLE_COUNT_1_BIT, format, VK_IMAGE_TILING_OPTIMAL, 
			      VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VMA_MEMORY_USAGE_GPU_ONLY, shadowcube.cubeMap.image, shadowcube.cubeMap.allocation, VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT, 6);

	resourceManager.transition_image_layout(device, m_graphicsCommandPool, m_graphicsQueue, shadowcube.cubeMap.image, format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1, 6);

	// Create sampler
	VkSamplerCreateInfo sampler{};
	sampler.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	sampler.magFilter = VK_FILTER_LINEAR;
	sampler.minFilter = VK_FILTER_LINEAR;
	sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	sampler.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
	sampler.addressModeV = sampler.addressModeU;
	sampler.addressModeW = sampler.addressModeU;
	sampler.mipLodBias = 0.0f;
	sampler.maxAnisotropy = 1.0f;
	sampler.compareOp = VK_COMPARE_OP_NEVER;
	sampler.minLod = 0.0f;
	sampler.maxLod = 1.0f;
	sampler.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
	VK_CHECK(vkCreateSampler(device, &sampler, nullptr, &shadowcube.cubemapSampler));

	// Create image view
	VkImageViewCreateInfo view{};
	view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	view.image = VK_NULL_HANDLE;
	view.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
	view.format = format;
	view.components = { VK_COMPONENT_SWIZZLE_R };
	view.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
	view.subresourceRange.layerCount = 6;
	view.image = shadowcube.cubeMap.image;
	VK_CHECK(vkCreateImageView(device, &view, nullptr, &shadowcube.cubeMap.imageView));

	view.viewType = VK_IMAGE_VIEW_TYPE_2D;
	view.subresourceRange.layerCount = 1;
	view.image = shadowcube.cubeMap.image;

	for (uint32_t i = 0; i < 6; i++)
	{
		view.subresourceRange.baseArrayLayer = i;
		VK_CHECK(vkCreateImageView(device, &view, nullptr, &shadowcube.faceImageViews[i]));
	}
}


void Renderer::create_depth_resources()
{
	VkFormat depthFormat = find_supported_format(
		{VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
		VK_IMAGE_TILING_OPTIMAL,
		VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
	);

	swapchainManager.create_depth_images(device, depthFormat, msaaSamples, resourceManager.get_allocator());


	VkImageCreateInfo imageInfo{};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.extent.width = shadowMapize;
	imageInfo.extent.height = shadowMapize;
	imageInfo.extent.depth = 1;
	imageInfo.mipLevels = 1;
	imageInfo.arrayLayers = 1;
	imageInfo.format = depthFormat;
	imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VmaAllocationCreateInfo allocInfo{};
	allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

	if (vmaCreateImage(resourceManager.get_allocator(), &imageInfo, &allocInfo, &shadowcube.depthImage.image, &shadowcube.depthImage.allocation, nullptr) != VK_SUCCESS) {
		std::cerr << "failed to create image with VMA!" << std::endl;
		abort();
	}

	resourceManager.transition_image_layout(device, m_graphicsCommandPool, m_graphicsQueue, shadowcube.depthImage.image, depthFormat, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, 1, 1, VK_IMAGE_ASPECT_DEPTH_BIT);

	/*resourceManager.create_image(shadowMapize, shadowMapize, 1, VK_SAMPLE_COUNT_1_BIT, depthFormat, VK_IMAGE_TILING_OPTIMAL,
	      VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_GPU_ONLY, shadowcube.depthImage.image, shadowcube.depthImage.allocation);
*/
	VkFormatProperties formatProps;
	vkGetPhysicalDeviceFormatProperties(m_physicalDevice, depthFormat, &formatProps);

	VkFilter shadowmapFilter = formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;

	// Create depth shadowmap image views and samplers
	/*VkSamplerCreateInfo shadowmapSampler{};
	shadowmapSampler.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	shadowmapSampler.magFilter = shadowmapFilter;
	shadowmapSampler.minFilter = shadowmapFilter;
	shadowmapSampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	shadowmapSampler.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	shadowmapSampler.addressModeV = shadowmapSampler.addressModeU;
	shadowmapSampler.addressModeW = shadowmapSampler.addressModeU;
	shadowmapSampler.mipLodBias = 0.0f;
	shadowmapSampler.maxAnisotropy = 1.0f;
	shadowmapSampler.minLod = 0.0f;
	shadowmapSampler.maxLod = 1.0f;
	shadowmapSampler.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
	VK_CHECK(vkCreateSampler(device, &shadowmapSampler, nullptr, &shadowcube.depthSampler));

	VkImageViewCreateInfo shadowmapViewInfo{};
	shadowmapViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	shadowmapViewInfo.image = shadowcube.depthImage.image;
	shadowmapViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	shadowmapViewInfo.format = depthFormat;
	shadowmapViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
	shadowmapViewInfo.subresourceRange.baseMipLevel = 0;
	shadowmapViewInfo.subresourceRange.levelCount = 1;
	shadowmapViewInfo.subresourceRange.baseArrayLayer = 0;
	shadowmapViewInfo.subresourceRange.layerCount = 1;

	vkCreateImageView(device, &shadowmapViewInfo, nullptr, &shadowcube.depthImage.imageView);
	*/

	// Create depth cubemap image views and samplers
	VkSamplerCreateInfo cubemapSampler{};
	cubemapSampler.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	cubemapSampler.magFilter = shadowmapFilter;
	cubemapSampler.minFilter = shadowmapFilter;
	cubemapSampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	cubemapSampler.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	cubemapSampler.addressModeV = cubemapSampler.addressModeU;
	cubemapSampler.addressModeW = cubemapSampler.addressModeU;
	cubemapSampler.mipLodBias = 0.0f;
	cubemapSampler.maxAnisotropy = 1.0f;
	cubemapSampler.minLod = 0.0f;
	cubemapSampler.maxLod = 1.0f;
	cubemapSampler.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
	VK_CHECK(vkCreateSampler(device, &cubemapSampler, nullptr, &shadowcube.depthSampler));

	VkImageViewCreateInfo viewInfo{};
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.image = shadowcube.depthImage.image;
	viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	viewInfo.format = depthFormat;
	viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
	if (depthFormat >= VK_FORMAT_D16_UNORM_S8_UINT) {
		viewInfo.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
	}
	viewInfo.subresourceRange.baseMipLevel = 0;
	viewInfo.subresourceRange.levelCount = 1;
	viewInfo.subresourceRange.baseArrayLayer = 0;
	viewInfo.subresourceRange.layerCount = 1;

	vkCreateImageView(device, &viewInfo, nullptr, &shadowcube.depthImage.imageView);
}

void Renderer::create_color_resources()
{
	swapchainManager.create_color_images(device, msaaSamples, resourceManager.get_allocator());
}

VkFormat Renderer::find_supported_format(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features)
{
	for (VkFormat format : candidates) {
		VkFormatProperties props;
		vkGetPhysicalDeviceFormatProperties(m_physicalDevice, format, &props);

		if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {
			return format;
		} else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
			return format;
		}
	}

	std::cerr << "Failed to find supported format!" << std::endl;
	abort();
}

void Renderer::create_descriptor_sets()
{
	m_descriptors.resize(MAX_FRAMES_IN_FLIGHT);
	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		std::vector<DescriptorAllocatorGrowable::PoolSizeRatio> frameSizes = { 
			{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 3 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3 },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3 },
			{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4 },
		};

		m_descriptors[i] = DescriptorAllocatorGrowable{};
		m_descriptors[i].init(device, 1000, frameSizes);
	}
}

uint32_t Renderer::find_memory_type(uint32_t typeFilter, VkMemoryPropertyFlags properties)
{
	VkPhysicalDeviceMemoryProperties memProperties;
	vkGetPhysicalDeviceMemoryProperties(m_physicalDevice, &memProperties);

	for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
		if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
			return i;
		}
	}

	std::cerr << "Failed to find suitable memory type" << std::endl;
	abort();
}

void Renderer::create_scene_data()
{
	m_gpuSceneDataBuffer = resourceManager.create_buffer(sizeof(GPUSceneData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, VK_SHARING_MODE_EXCLUSIVE, m_queueFamilies);
	m_offscrenSceneDataBuffer = resourceManager.create_buffer(sizeof(OffscreenSceneData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, VK_SHARING_MODE_EXCLUSIVE, m_queueFamilies);
}
void Renderer::create_command_buffers()
{
	m_commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);
	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = m_graphicsCommandPool;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandBufferCount = static_cast<uint32_t>(m_commandBuffers.size()) ;

	VK_CHECK(vkAllocateCommandBuffers(device, &allocInfo, m_commandBuffers.data()));
}

void Renderer::record_command_buffer(VkCommandBuffer commandBuffer, uint32_t imageIndex, ImDrawData* drawData) {

	//reset counters
	stats.drawcallCount = 0;
	stats.triangleCount = 0;
	//begin clock
	auto start = std::chrono::system_clock::now();

	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = 0;
	beginInfo.pInheritanceInfo = nullptr;

	VK_CHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo));

	VkViewport viewport{};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = static_cast<float>(shadowMapize);
	viewport.height = static_cast<float>(shadowMapize);
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

	VkRect2D scissor{};
	scissor.offset = {0, 0};
	scissor.extent.width = shadowMapize;
	scissor.extent.height = shadowMapize;
	vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

	//create a descriptor set that binds that buffer and update it
	VkDescriptorSet globalDescriptorOffscreen = m_descriptors[m_currentFrame].allocate(device, m_offscreenSceneDataDescriptorLayout);
	DescriptorWriter writer;

	writer.write_buffer(0, m_offscrenSceneDataBuffer.buffer, sizeof(OffscreenSceneData), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	writer.update_set(device, globalDescriptorOffscreen);

	void* data;
	vmaMapMemory(resourceManager.get_allocator(), m_offscrenSceneDataBuffer.allocation, &data);
	memcpy(data, &m_offscreenData, static_cast<size_t>(sizeof(OffscreenSceneData)));
	vmaUnmapMemory(resourceManager.get_allocator(), m_offscrenSceneDataBuffer.allocation);

	std::array<VkClearValue, 2> clearValues;

	/*
		First render pass: Generate shadow map by rendering the scene from light's POV
	*/
	/*{
		clearValues[0].depthStencil = { 1.0f, 0 };

		VkRenderPassBeginInfo renderPassBeginInfo{};
		renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassBeginInfo.renderPass = shadow.renderPass;
		renderPassBeginInfo.framebuffer = shadow.framebuffer;
		renderPassBeginInfo.renderArea.extent.width = shadowMapize;
		renderPassBeginInfo.renderArea.extent.height = shadowMapize;
		renderPassBeginInfo.clearValueCount = 1;
		renderPassBeginInfo.pClearValues = clearValues.data();

		vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

		vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
		vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

		// Depth bias (and slope) are used to avoid shadowing artifacts
		// Constant depth bias factor (always applied)
		float depthBiasConstant = 1.25f;
		// Slope depth bias factor, applied depending on polygon's slope
		float depthBiasSlope = 1.75f;

		// Set depth bias (aka "Polygon offset")
		// Required to avoid shadow mapping artifacts
		vkCmdSetDepthBias(
			commandBuffer,
			depthBiasConstant,
			0.0f,
		depthBiasSlope);

		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, shadow.pipeline.pipeline);

		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, shadow.pipeline.layout, 0, 1, &globalDescriptor, 0, nullptr);

		for (const RenderObject& obj : m_mainDrawContext.opaqueSurfaces) {
			vkCmdBindIndexBuffer(commandBuffer, obj.indexBuffer, 0, VK_INDEX_TYPE_UINT32);

			float near_plane = 1.0f, far_plane = 7.5f;
			glm::mat4 lightProjection = glm::ortho(-10.0f, 10.0f, -10.0f, 10.0f, near_plane, far_plane);

			PushConstants pushConstants;
			pushConstants.vertexBuffer = obj.vertexBufferAddress;
			pushConstants.worldMatrix = obj.transform;
			pushConstants.lightSpaceMatrix = lightProjection * lightViewMatrix;
			vkCmdPushConstants(commandBuffer, shadow.pipeline.layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstants), &pushConstants);

			vkCmdDrawIndexed(commandBuffer, obj.indexCount, 1, obj.firstIndex,0,0);
		}

		vkCmdEndRenderPass(commandBuffer);
	}*/

	/*
		First render pass: Generate cube map by rendering the scene from light's POV in all directions
	*/
	{
		vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
		vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

		for (uint32_t face = 0; face < 6; face++) {
			update_cube_face(face, commandBuffer, globalDescriptorOffscreen);
		}
	}

	VkDescriptorSet globalDescriptor = m_descriptors[m_currentFrame].allocate(device, m_gpuSceneDataDescriptorLayout);

	writer.write_buffer(0, m_gpuSceneDataBuffer.buffer, sizeof(GPUSceneData), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	writer.update_set(device, globalDescriptor);

	vmaMapMemory(resourceManager.get_allocator(), m_gpuSceneDataBuffer.allocation, &data);
	memcpy(data, &m_sceneData, static_cast<size_t>(sizeof(GPUSceneData)));
	vmaUnmapMemory(resourceManager.get_allocator(), m_gpuSceneDataBuffer.allocation);

	VkRenderPassBeginInfo renderPassInfo{};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassInfo.renderPass = renderPass;
	renderPassInfo.framebuffer = swapchainManager.get_swapchain_framebuffer(imageIndex);
	renderPassInfo.renderArea.offset = { 0, 0 };
	renderPassInfo.renderArea.extent = swapchainManager.get_swapchain_extent();

	clearValues[0].color = {{0.0f, 0.0f, 0.0f, 1.0f}};
	clearValues[1].depthStencil = {1.0f, 0};
	renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
	renderPassInfo.pClearValues = clearValues.data();

	VkSubpassBeginInfo subpassBeginInfo{};
	subpassBeginInfo.sType = VK_STRUCTURE_TYPE_SUBPASS_BEGIN_INFO;
	subpassBeginInfo.contents = VK_SUBPASS_CONTENTS_INLINE;

	VkSubpassEndInfo subpassEndInfo{};
	subpassEndInfo.sType = VK_STRUCTURE_TYPE_SUBPASS_END_INFO;

	vkCmdBeginRenderPass2(commandBuffer, &renderPassInfo, &subpassBeginInfo);

	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_defaultData.pipeline->pipeline);


	MaterialPipeline* lastPipeline = nullptr;
	MaterialInstance* lastMaterial = nullptr;
	VkBuffer lastIndexBuffer = VK_NULL_HANDLE;

	auto draw = [&](const RenderObject& r) {

		if (r.material != lastMaterial) {
			lastMaterial = r.material;
			//rebind pipeline and descriptors if the material changed
			if (r.material->pipeline != lastPipeline) {
				lastPipeline = r.material->pipeline;
				vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, r.material->pipeline->pipeline);
				vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, r.material->pipeline->layout, 0, 1, &globalDescriptor, 0, nullptr );

				VkViewport viewport = {};
				viewport.x = 0;
				viewport.y = 0;
				viewport.width = (float)swapchainManager.get_swapchain_extent().width;
				viewport.height = (float)swapchainManager.get_swapchain_extent().height;
				viewport.minDepth = 0.f;
				viewport.maxDepth = 1.f;

				vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

				VkRect2D scissor = {};
				scissor.offset.x = 0;
				scissor.offset.y = 0;
				scissor.extent.width = swapchainManager.get_swapchain_extent().width;
				scissor.extent.height = swapchainManager.get_swapchain_extent().height;

				vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
			}
			vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, r.material->pipeline->layout, 1, 1, &r.material->materialSet, 0, nullptr);
		}

		//rebind index buffer if needed
		if (r.indexBuffer != lastIndexBuffer) {
			lastIndexBuffer = r.indexBuffer;
			vkCmdBindIndexBuffer(commandBuffer, r.indexBuffer, 0, VK_INDEX_TYPE_UINT32);
		}

		float near_plane = 1.0f, far_plane = 7.5f;
		glm::mat4 lightProjection = glm::ortho(-10.0f, 10.0f, -10.0f, 10.0f, near_plane, far_plane);


		PushConstants pushConstants;
		pushConstants.vertexBuffer = r.vertexBufferAddress;
		pushConstants.worldMatrix = r.transform;
		pushConstants.lightSpaceMatrix = lightProjection * lightViewMatrix;
		pushConstants.alphaCutoff = r.alphaCutoff;
		vkCmdPushConstants(commandBuffer, r.material->pipeline->layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstants), &pushConstants);

		vkCmdDrawIndexed(commandBuffer, r.indexCount, 1, r.firstIndex, 0, 0);

		//add counters for triangles and draws
		stats.drawcallCount++;
		stats.triangleCount += r.indexCount / 3;
	};


	for (const RenderObject& obj : m_mainDrawContext.opaqueSurfaces) {
		draw(obj);
	}

	for (const RenderObject& obj : m_mainDrawContext.transparentSurfaces) {
		draw(obj);
	}

	ImGui_ImplVulkan_RenderDrawData(drawData, commandBuffer);

	vkCmdEndRenderPass2(commandBuffer, &subpassEndInfo);

	VK_CHECK(vkEndCommandBuffer(commandBuffer));

	auto end = std::chrono::system_clock::now();

	auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
	stats.meshDrawTime = elapsed.count() / 1000.f;

}

void Renderer::update_cube_face(uint32_t faceIndex, VkCommandBuffer commandBuffer, VkDescriptorSet descriptor)
{
	VkClearValue clearValues[2];
	clearValues[0].color = { { 0.0f, 0.0f, 0.0f, 1.0f } };
	clearValues[1].depthStencil = { 1.0f, 0 };

	VkRenderPassBeginInfo renderPassBeginInfo{};
	renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	// Reuse render pass from example pass
	renderPassBeginInfo.renderPass = shadowcube.renderPass;
	renderPassBeginInfo.framebuffer = shadowcube.framebuffers[faceIndex];
	renderPassBeginInfo.renderArea.extent.width = shadowMapize;
	renderPassBeginInfo.renderArea.extent.height = shadowMapize;
	renderPassBeginInfo.clearValueCount = 2;
	renderPassBeginInfo.pClearValues = clearValues;

	
	float nearPlane = 1.0f;

	// Update view matrix via push constant
        glm::mat4 shadowProj = glm::perspective(glm::radians(90.0f), 1.0f, nearPlane, m_offscreenData.farPlane);
	glm::mat4 lightTransform = glm::mat4(1.0f);

	glm::vec3 lightPos = m_offscreenData.lightPos;

	switch (faceIndex)
	{
		case 0: // POSITIVE_X
			lightTransform = shadowProj * glm::lookAt(lightPos, lightPos + glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f));
		break;
		case 1:	// NEGATIVE_X
			lightTransform = shadowProj * glm::lookAt(lightPos, lightPos + glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f));
		break;
		case 2:	// POSITIVE_Y
			lightTransform = shadowProj * glm::lookAt(lightPos, lightPos + glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
		break;
		case 3:	// NEGATIVE_Y
			lightTransform = shadowProj * glm::lookAt(lightPos, lightPos + glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f));
		break;
		case 4:	// POSITIVE_Z
			lightTransform = shadowProj * glm::lookAt(lightPos, lightPos + glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, -1.0f, 0.0f));
		break;
		case 5:	// NEGATIVE_Z
			lightTransform = shadowProj * glm::lookAt(lightPos, lightPos + glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, -1.0f, 0.0f));
		break;
	}

	// Render scene from cube face's point of view
	vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, shadowcube.pipeline.pipeline);
	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, shadowcube.pipeline.layout, 0, 1, &descriptor, 0, NULL);

	MaterialInstance* lastMaterial = nullptr;

	for (const RenderObject& obj : m_mainDrawContext.opaqueSurfaces) {
		vkCmdBindIndexBuffer(commandBuffer, obj.indexBuffer, 0, VK_INDEX_TYPE_UINT32);

		if (obj.material != lastMaterial) {
			lastMaterial = obj.material;
			vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, shadowcube.pipeline.layout, 1, 1, &obj.shadowMaterial->materialSet, 0, nullptr);
		}

		PushConstants pushConstants;
		pushConstants.vertexBuffer = obj.vertexBufferAddress;
		pushConstants.worldMatrix = obj.transform;
		pushConstants.lightSpaceMatrix = lightTransform;
		pushConstants.alphaCutoff = obj.alphaCutoff;

		vkCmdPushConstants(commandBuffer, shadowcube.pipeline.layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstants), &pushConstants);

		vkCmdDrawIndexed(commandBuffer, obj.indexCount, 1, obj.firstIndex,0,0);
	}

	vkCmdEndRenderPass(commandBuffer);
}


void Renderer::create_sync_objects()
{
	m_imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
	m_renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
	m_inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

	VkSemaphoreCreateInfo semaphoreInfo{};
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	VkFenceCreateInfo fenceInfo{};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		VK_CHECK(vkCreateSemaphore(device, &semaphoreInfo, nullptr, &m_imageAvailableSemaphores[i]));
		VK_CHECK(vkCreateSemaphore(device, &semaphoreInfo, nullptr, &m_renderFinishedSemaphores[i]));
		VK_CHECK(vkCreateFence(device, &fenceInfo, nullptr, &m_inFlightFences[i]));
	}

}

void Renderer::update_loaded_scenes(Scene& scene)
{
	auto& registry = scene.get_registry();

	auto view = registry.view<MeshComponent, TransformComponent>();

	for (auto entity : view) {
		const auto& mesh = view.get<MeshComponent>(entity);
		auto structureFile = loadGltf(mesh.meshPath);

		assert(structureFile.has_value());

		m_loadedScenes[entity] = *structureFile;
	}
}

VkSampleCountFlagBits Renderer::get_max_usable_sample_count()
{
	VkPhysicalDeviceProperties physicalDeviceProperties;
	vkGetPhysicalDeviceProperties(m_physicalDevice, &physicalDeviceProperties);

	VkSampleCountFlags counts = physicalDeviceProperties.limits.framebufferColorSampleCounts & physicalDeviceProperties.limits.framebufferDepthSampleCounts;
	if (counts & VK_SAMPLE_COUNT_64_BIT) { return VK_SAMPLE_COUNT_64_BIT; }
	if (counts & VK_SAMPLE_COUNT_32_BIT) { return VK_SAMPLE_COUNT_32_BIT; }
	if (counts & VK_SAMPLE_COUNT_16_BIT) { return VK_SAMPLE_COUNT_16_BIT; }
	if (counts & VK_SAMPLE_COUNT_8_BIT) { return VK_SAMPLE_COUNT_8_BIT; }
	if (counts & VK_SAMPLE_COUNT_4_BIT) { return VK_SAMPLE_COUNT_4_BIT; }
	if (counts & VK_SAMPLE_COUNT_2_BIT) { return VK_SAMPLE_COUNT_2_BIT; }

	return VK_SAMPLE_COUNT_1_BIT;
}

AllocatedBuffer Renderer::create_buffer(VkDeviceSize size, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage, VkSharingMode sharingMode)
{
	return resourceManager.create_buffer(size, usage, memoryUsage, sharingMode, m_queueFamilies);
}

AllocatedImage Renderer::create_image(void* data, VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped)
{
	return resourceManager.create_image(device, m_graphicsCommandPool, m_graphicsQueue, m_physicalDevice, data, size, format, usage, mipmapped, m_queueFamilies);
}

GPUMeshBuffers Renderer::upload_mesh(std::vector<uint32_t>& indices, std::vector<Vertex>& vertices)
{
	const size_t vertexBufferSize = vertices.size() * sizeof(Vertex);
	const size_t indexBufferSize = indices.size() * sizeof(uint32_t);

	GPUMeshBuffers newSurface;

	AllocatedBuffer vertexStagingBuffer = resourceManager.create_buffer(vertexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, VK_SHARING_MODE_EXCLUSIVE, m_queueFamilies);

	void* vertexData;
	vmaMapMemory(resourceManager.get_allocator(), vertexStagingBuffer.allocation, &vertexData);
	memcpy(vertexData, vertices.data(), static_cast<size_t>(vertexBufferSize));
	vmaUnmapMemory(resourceManager.get_allocator(), vertexStagingBuffer.allocation);

	newSurface.vertexBuffer = resourceManager.create_buffer(vertexBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VMA_MEMORY_USAGE_GPU_ONLY, VK_SHARING_MODE_CONCURRENT, m_queueFamilies);

	VkBufferDeviceAddressInfo deviceAddressInfo{ .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, .buffer = newSurface.vertexBuffer.buffer };
	newSurface.vertexBufferAddress = vkGetBufferDeviceAddress(device, &deviceAddressInfo);

	resourceManager.copy_buffer(device, m_graphicsCommandPool, m_graphicsQueue, vertexStagingBuffer.buffer, newSurface.vertexBuffer.buffer, vertexBufferSize);

	resourceManager.destroy_buffer(vertexStagingBuffer);

	AllocatedBuffer indexStagingBuffer = resourceManager.create_buffer(indexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,  VMA_MEMORY_USAGE_CPU_TO_GPU, VK_SHARING_MODE_EXCLUSIVE, m_queueFamilies);

	void* indexData;
	vmaMapMemory(resourceManager.get_allocator(), indexStagingBuffer.allocation, &indexData);
	memcpy(indexData, indices.data(), static_cast<size_t>(indexBufferSize));
	vmaUnmapMemory(resourceManager.get_allocator(), indexStagingBuffer.allocation);

	newSurface.indexBuffer = resourceManager.create_buffer(indexBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VMA_MEMORY_USAGE_GPU_ONLY, VK_SHARING_MODE_EXCLUSIVE, m_queueFamilies);

	resourceManager.copy_buffer(device, m_graphicsCommandPool, m_graphicsQueue, indexStagingBuffer.buffer, newSurface.indexBuffer.buffer, indexBufferSize);

	resourceManager.destroy_buffer(indexStagingBuffer);

	return newSurface;
}

void Renderer::update_scene(const Camera& camera)
{
	auto start = std::chrono::system_clock::now();

	m_mainDrawContext.opaqueSurfaces.clear();
	m_mainDrawContext.transparentSurfaces.clear();

	auto& registry = scene->get_registry();

	auto meshView = registry.view<TransformComponent, MeshComponent>();
	for (auto entity : meshView) {
		const auto& transform = meshView.get<TransformComponent>(entity);
		glm::mat4 model = glm::mat4(1.0f);

		model = glm::translate(model, transform.position);

		model = glm::rotate(model, glm::radians(transform.rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
		model = glm::rotate(model, glm::radians(transform.rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
		model = glm::rotate(model, glm::radians(transform.rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));

		model = glm::scale(model, transform.scale);

		m_loadedScenes[entity]->draw(model, m_mainDrawContext);
	}

	m_sceneData.view = camera.get_view_matrix();
	// camera projection
	m_sceneData.proj = camera.get_projection_matrix(swapchainManager.get_swapchain_extent().width, swapchainManager.get_swapchain_extent().height);

	m_sceneData.camPos = glm::vec4(camera.position, 0.0f);

	m_sceneData.viewproj = m_sceneData.proj * m_sceneData.view;

	float near_plane = 1.0f, far_plane = 7.5f;
	glm::mat4 lightProjection = glm::ortho(-10.0f, 10.0f, -10.0f, 10.0f, near_plane, far_plane);

	m_sceneData.lightSpaceMatrix = lightProjection * lightViewMatrix;

	//some default lighting parameters
	m_sceneData.ambientColor = glm::vec4(.1f);
	m_sceneData.sunlightColor = glm::vec4(1.f);
	m_sceneData.sunlightDirection = glm::normalize(glm::vec4(1, 1, 0.5, 0.f));

	float zFar = 25.0f;
	m_sceneData.farPlane = zFar;

	auto lightView = registry.view<TransformComponent, LightComponent>();
	unsigned int i = 0;
	for (auto entity : lightView) {
		const auto& transform = lightView.get<TransformComponent>(entity);
		const auto& light = lightView.get<LightComponent>(entity);
		m_sceneData.lightColors[i] = glm::vec4(light.color, 0.0f);
		m_sceneData.lightPos[i] = glm::vec4(transform.position, 0.0f);

		i++;
	}

	// TODO: update to multiple lightpos in cubemap
	glm::vec4 lightPos = m_sceneData.lightPos[0];


	m_offscreenData.lightPos = lightPos;
	m_offscreenData.farPlane = zFar;

	auto end = std::chrono::system_clock::now();
	auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

	stats.sceneUpdateTime = elapsed.count() / 1000.f;
}



[[nodiscard]] VkDescriptorSetLayout Renderer::get_gpu_scene_data_descriptor_layout() const
{
	return m_gpuSceneDataDescriptorLayout;
}

[[nodiscard]] VkDescriptorSetLayout Renderer::get_material_descriptor_layout() const
{
	return m_materialDescriptorLayout;
}

void ShadowCube::build_pipelines(Renderer& renderer)
{
	auto shadowFragShaderCode = read_file("../../shaders/shadowcube.frag.spv");
	auto shadowVertexShaderCode = read_file("../../shaders/shadowcube.vert.spv");

	VkShaderModule shadowFragShader = create_shader_module(renderer.device, shadowFragShaderCode);
	VkShaderModule shadowVertexShader = create_shader_module(renderer.device, shadowVertexShaderCode);

	VkPushConstantRange matrixRange{};
	matrixRange.offset = 0;
	matrixRange.size = sizeof(PushConstants);
	matrixRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

	VkDescriptorSetLayoutBinding uboLayoutBinding{};
	uboLayoutBinding.binding = 0;
	uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	uboLayoutBinding.descriptorCount = 1;
	uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
	uboLayoutBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutBinding samplerLayoutBinding{};
	samplerLayoutBinding.binding = 1;
	samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	samplerLayoutBinding.descriptorCount = 1;
	samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	samplerLayoutBinding.pImmutableSamplers = nullptr;

	std::array<VkDescriptorSetLayoutBinding, 2> bindings = { uboLayoutBinding, samplerLayoutBinding };
	VkDescriptorSetLayoutCreateInfo layoutInfo{};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
	layoutInfo.pBindings = bindings.data();

	VK_CHECK(vkCreateDescriptorSetLayout(renderer.device, &layoutInfo, nullptr, &shadowLayout));

	VkDescriptorSetLayout layouts[] = { renderer.get_gpu_scene_data_descriptor_layout(), shadowLayout };
	
	VkPipelineLayoutCreateInfo meshLayoutInfo{};
	meshLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	meshLayoutInfo.setLayoutCount = 2;
	meshLayoutInfo.pSetLayouts = layouts;
	meshLayoutInfo.pPushConstantRanges = &matrixRange;
	meshLayoutInfo.pushConstantRangeCount = 1;

	VkPipelineLayout newShadowcubePipeline;
	VK_CHECK(vkCreatePipelineLayout(renderer.device, &meshLayoutInfo, nullptr, &newShadowcubePipeline));

	VkViewport viewport{};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = (float) renderer.shadowMapize;
	viewport.height = (float) renderer.shadowMapize;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	VkRect2D scissor{};
	scissor.offset = { 0, 0 };
	scissor.extent.width = renderer.shadowMapize;
	scissor.extent.height = renderer.shadowMapize;

	VkPipelineViewportStateCreateInfo viewportState{};
	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.viewportCount = 1;
	viewportState.pViewports = &viewport;
	viewportState.scissorCount = 1;
	viewportState.pScissors = &scissor;

	pipeline.layout = newShadowcubePipeline;

	VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
	vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertShaderStageInfo.module = shadowVertexShader;
	vertShaderStageInfo.pName = "main";

	VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
	fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragShaderStageInfo.module = shadowFragShader;
	fragShaderStageInfo.pName = "main";

	VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

	std::vector<VkDynamicState> dynamicStates = {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR
	};

	VkPipelineDynamicStateCreateInfo dynamicState{};
	dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
	dynamicState.pDynamicStates = dynamicStates.data();

	VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
	inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssembly.primitiveRestartEnable = VK_FALSE;

	VkPipelineRasterizationStateCreateInfo rasterizer{};
	rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer.depthClampEnable = VK_FALSE;
	rasterizer.rasterizerDiscardEnable = VK_FALSE;
	rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizer.lineWidth = 1.0f;
	rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
	rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rasterizer.depthBiasEnable = VK_FALSE;
	rasterizer.depthBiasConstantFactor = 0.0f;
	rasterizer.depthBiasClamp = 0.0f;
	rasterizer.depthBiasSlopeFactor = 0.0f;

	VkPipelineMultisampleStateCreateInfo multisampling{};
	multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.sampleShadingEnable = VK_FALSE;
	multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	VkPipelineColorBlendAttachmentState colorBlendAttachment{};
	colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	colorBlendAttachment.blendEnable = VK_FALSE;

	VkPipelineColorBlendStateCreateInfo colorBlending = {};
	colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlending.pNext = nullptr;
	colorBlending.logicOpEnable = VK_FALSE;
	colorBlending.logicOp = VK_LOGIC_OP_COPY;
	colorBlending.attachmentCount = 1;
	colorBlending.pAttachments = &colorBlendAttachment;

	VkPipelineDepthStencilStateCreateInfo depthStencil{};
	depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencil.depthTestEnable = VK_TRUE;
	depthStencil.depthWriteEnable = VK_TRUE;
	depthStencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
	depthStencil.depthBoundsTestEnable = VK_FALSE;
	depthStencil.stencilTestEnable = VK_FALSE;
	depthStencil.front = {};
	depthStencil.back = {};
	depthStencil.minDepthBounds = 0.f;
	depthStencil.maxDepthBounds = 1.f;

	VkPipelineVertexInputStateCreateInfo vertexInputInfo = { .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };

	VkGraphicsPipelineCreateInfo pipelineInfo{};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineInfo.stageCount = 2;
	pipelineInfo.pStages = shaderStages;
	pipelineInfo.pVertexInputState = &vertexInputInfo;
	pipelineInfo.pInputAssemblyState = &inputAssembly;
	pipelineInfo.pViewportState = &viewportState;
	pipelineInfo.pRasterizationState = &rasterizer;
	pipelineInfo.pMultisampleState = &multisampling;
	pipelineInfo.pDepthStencilState = &depthStencil;
	pipelineInfo.pColorBlendState = &colorBlending;
	pipelineInfo.pDynamicState = &dynamicState;
	pipelineInfo.renderPass = renderPass;
	pipelineInfo.layout = newShadowcubePipeline;

	VK_CHECK(vkCreateGraphicsPipelines(renderer.device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline.pipeline));

	vkDestroyShaderModule(renderer.device, shadowFragShader , nullptr);
	vkDestroyShaderModule(renderer.device, shadowVertexShader, nullptr);
}

void ShadowCube::clear_resources(VkDevice device)
{
	vkDestroyPipeline(device, pipeline.pipeline, nullptr);
	vkDestroyPipelineLayout(device, pipeline.layout, nullptr);

	vkDestroyDescriptorSetLayout(device, shadowLayout, nullptr);
}

ShadowCubeInstance ShadowCube::write_material(VkDevice device, const ShadowResources& resources, DescriptorAllocatorGrowable& descriptorAllocator)
{

	ShadowCubeInstance matData;
	matData.pipeline = &pipeline;
	matData.materialSet = descriptorAllocator.allocate(device, shadowLayout);

	writer.clear();
	writer.write_buffer(0, resources.dataBuffer, sizeof(ShadowConstants), resources.dataBufferOffset, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	writer.write_image(1, resources.colorImage.imageView, resources.colorSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

	writer.update_set(device, matData.materialSet);

	return matData;
}

void Shadow::build_pipelines(Renderer& renderer)
{
	auto shadowFragShaderCode = read_file("../../shaders/shadow.frag.spv");
	auto shadowVertexShaderCode = read_file("../../shaders/shadow.vert.spv");

	VkShaderModule shadowFragShader = create_shader_module(renderer.device, shadowFragShaderCode);
	VkShaderModule shadowVertexShader = create_shader_module(renderer.device, shadowVertexShaderCode);

	VkPushConstantRange matrixRange{};
	matrixRange.offset = 0;
	matrixRange.size = sizeof(PushConstants);
	matrixRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

	VkDescriptorSetLayoutBinding uboLayoutBinding{};
	uboLayoutBinding.binding = 0;
	uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	uboLayoutBinding.descriptorCount = 1;
	uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
	uboLayoutBinding.pImmutableSamplers = nullptr;

	std::array<VkDescriptorSetLayoutBinding, 1> bindings = { uboLayoutBinding };
	VkDescriptorSetLayoutCreateInfo layoutInfo{};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
	layoutInfo.pBindings = bindings.data();

	VK_CHECK(vkCreateDescriptorSetLayout(renderer.device, &layoutInfo, nullptr, &shadowLayout));

	VkDescriptorSetLayout layouts[] = { renderer.get_gpu_scene_data_descriptor_layout(), shadowLayout };
	
	VkPipelineLayoutCreateInfo meshLayoutInfo{};
	meshLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	meshLayoutInfo.setLayoutCount = 2;
	meshLayoutInfo.pSetLayouts = layouts;
	meshLayoutInfo.pPushConstantRanges = &matrixRange;
	meshLayoutInfo.pushConstantRangeCount = 1;

	VkPipelineLayout newShadowPipeline;
	VK_CHECK(vkCreatePipelineLayout(renderer.device, &meshLayoutInfo, nullptr, &newShadowPipeline));

	VkViewport viewport{};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = (float) renderer.shadowMapize;
	viewport.height = (float) renderer.shadowMapize;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	VkRect2D scissor{};
	scissor.offset = { 0, 0 };
	scissor.extent.width = renderer.shadowMapize;
	scissor.extent.height = renderer.shadowMapize;

	VkPipelineViewportStateCreateInfo viewportState{};
	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.viewportCount = 1;
	viewportState.pViewports = &viewport;
	viewportState.scissorCount = 1;
	viewportState.pScissors = &scissor;

	pipeline.layout = newShadowPipeline;

	VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
	vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertShaderStageInfo.module = shadowVertexShader;
	vertShaderStageInfo.pName = "main";

	VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
	fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragShaderStageInfo.module = shadowFragShader;
	fragShaderStageInfo.pName = "main";

	VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

	std::vector<VkDynamicState> dynamicStates = {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR
	};

	VkPipelineDynamicStateCreateInfo dynamicState{};
	dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
	dynamicState.pDynamicStates = dynamicStates.data();

	VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
	inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssembly.primitiveRestartEnable = VK_FALSE;

	VkPipelineRasterizationStateCreateInfo rasterizer{};
	rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer.depthClampEnable = VK_FALSE;
	rasterizer.rasterizerDiscardEnable = VK_FALSE;
	rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizer.lineWidth = 1.0f;
	rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
	rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rasterizer.depthBiasEnable = VK_FALSE;
	rasterizer.depthBiasConstantFactor = 0.0f;
	rasterizer.depthBiasClamp = 0.0f;
	rasterizer.depthBiasSlopeFactor = 0.0f;

	VkPipelineMultisampleStateCreateInfo multisampling{};
	multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.sampleShadingEnable = VK_FALSE;
	multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	VkPipelineColorBlendAttachmentState colorBlendAttachment{};
	colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	colorBlendAttachment.blendEnable = VK_FALSE;

	VkPipelineColorBlendStateCreateInfo colorBlending = {};
	colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlending.pNext = nullptr;
	colorBlending.logicOpEnable = VK_FALSE;
	colorBlending.logicOp = VK_LOGIC_OP_COPY;
	colorBlending.attachmentCount = 1;
	colorBlending.pAttachments = &colorBlendAttachment;

	VkPipelineDepthStencilStateCreateInfo depthStencil{};
	depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencil.depthTestEnable = VK_TRUE;
	depthStencil.depthWriteEnable = VK_TRUE;
	depthStencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
	depthStencil.depthBoundsTestEnable = VK_FALSE;
	depthStencil.stencilTestEnable = VK_FALSE;
	depthStencil.front = {};
	depthStencil.back = {};
	depthStencil.minDepthBounds = 0.f;
	depthStencil.maxDepthBounds = 1.f;

	VkPipelineVertexInputStateCreateInfo vertexInputInfo = { .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };

	VkGraphicsPipelineCreateInfo pipelineInfo{};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineInfo.stageCount = 2;
	pipelineInfo.pStages = shaderStages;
	pipelineInfo.pVertexInputState = &vertexInputInfo;
	pipelineInfo.pInputAssemblyState = &inputAssembly;
	pipelineInfo.pViewportState = &viewportState;
	pipelineInfo.pRasterizationState = &rasterizer;
	pipelineInfo.pMultisampleState = &multisampling;
	pipelineInfo.pDepthStencilState = &depthStencil;
	pipelineInfo.pColorBlendState = &colorBlending;
	pipelineInfo.pDynamicState = &dynamicState;
	pipelineInfo.renderPass = renderPass;
	pipelineInfo.layout = newShadowPipeline;

	VK_CHECK(vkCreateGraphicsPipelines(renderer.device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline.pipeline));

	vkDestroyShaderModule(renderer.device, shadowFragShader , nullptr);
	vkDestroyShaderModule(renderer.device, shadowVertexShader, nullptr);
}

void Shadow::clear_resources(VkDevice device)
{
	vkDestroyPipeline(device, pipeline.pipeline, nullptr);
	vkDestroyPipelineLayout(device, pipeline.layout, nullptr);

	vkDestroyDescriptorSetLayout(device, shadowLayout, nullptr);
}

ShadowInstance Shadow::write_material(VkDevice device, const ShadowResources& resources, DescriptorAllocatorGrowable& descriptorAllocator)
{

	ShadowInstance matData;
	matData.pipeline = &pipeline;
	matData.materialSet = descriptorAllocator.allocate(device, shadowLayout);

	writer.clear();
	writer.write_buffer(0, resources.dataBuffer, sizeof(ShadowConstants), resources.dataBufferOffset, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);

	writer.update_set(device, matData.materialSet);

	return matData;
}

void GLTFMetallic_Roughness::build_pipelines(Renderer& renderer)
{
	auto meshFragShaderCode = read_file("../../shaders/shadowcube_colors.frag.spv");
	auto meshVertexShaderCode = read_file("../../shaders/shadowcube_colors.vert.spv");

	VkShaderModule meshFragShader = create_shader_module(renderer.device, meshFragShaderCode);
	VkShaderModule meshVertexShader = create_shader_module(renderer.device, meshVertexShaderCode);

	VkPushConstantRange matrixRange{};
	matrixRange.offset = 0;
	matrixRange.size = sizeof(PushConstants);
	matrixRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

	VkDescriptorSetLayoutBinding uboLayoutBinding{};
	uboLayoutBinding.binding = 0;
	uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	uboLayoutBinding.descriptorCount = 1;
	uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
	uboLayoutBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutBinding samplerLayoutBinding1{};
	samplerLayoutBinding1.binding = 1;
	samplerLayoutBinding1.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	samplerLayoutBinding1.descriptorCount = 1;
	samplerLayoutBinding1.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	samplerLayoutBinding1.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutBinding samplerLayoutBinding2{};
	samplerLayoutBinding2.binding = 2;
	samplerLayoutBinding2.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	samplerLayoutBinding2.descriptorCount = 1;
	samplerLayoutBinding2.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	samplerLayoutBinding2.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutBinding samplerLayoutBinding3{};
	samplerLayoutBinding3.binding = 3;
	samplerLayoutBinding3.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	samplerLayoutBinding3.descriptorCount = 1;
	samplerLayoutBinding3.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	samplerLayoutBinding3.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutBinding samplerLayoutBinding4{};
	samplerLayoutBinding4.binding = 4;
	samplerLayoutBinding4.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	samplerLayoutBinding4.descriptorCount = 1;
	samplerLayoutBinding4.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	samplerLayoutBinding4.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutBinding samplerLayoutBinding5{};
	samplerLayoutBinding5.binding = 5;
	samplerLayoutBinding5.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	samplerLayoutBinding5.descriptorCount = 1;
	samplerLayoutBinding5.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	samplerLayoutBinding5.pImmutableSamplers = nullptr;

	std::array<VkDescriptorSetLayoutBinding, 6> bindings = { uboLayoutBinding, samplerLayoutBinding1, samplerLayoutBinding2, samplerLayoutBinding3, samplerLayoutBinding4, samplerLayoutBinding5 };
	VkDescriptorSetLayoutCreateInfo layoutInfo{};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
	layoutInfo.pBindings = bindings.data();

	VK_CHECK(vkCreateDescriptorSetLayout(renderer.device, &layoutInfo, nullptr, &materialLayout));

	VkDescriptorSetLayout layouts[] = { renderer.get_gpu_scene_data_descriptor_layout(), materialLayout };
	
	VkPipelineLayoutCreateInfo meshLayoutInfo{};
	meshLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	meshLayoutInfo.setLayoutCount = 2;
	meshLayoutInfo.pSetLayouts = layouts;
	meshLayoutInfo.pPushConstantRanges = &matrixRange;
	meshLayoutInfo.pushConstantRangeCount = 1;

	VkPipelineLayout newOpaqueLayout;
	VkPipelineLayout newTransparentLayout;
	VK_CHECK(vkCreatePipelineLayout(renderer.device, &meshLayoutInfo, nullptr, &newOpaqueLayout));
	VK_CHECK(vkCreatePipelineLayout(renderer.device, &meshLayoutInfo, nullptr, &newTransparentLayout));

	VkViewport viewport{};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = (float) renderer.swapchainManager.get_swapchain_extent().width;
	viewport.height = (float) renderer.swapchainManager.get_swapchain_extent().height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	VkRect2D scissor{};
	scissor.offset = { 0, 0 };
	scissor.extent = renderer.swapchainManager.get_swapchain_extent();

	VkPipelineViewportStateCreateInfo viewportState{};
	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.viewportCount = 1;
	viewportState.pViewports = &viewport;
	viewportState.scissorCount = 1;
	viewportState.pScissors = &scissor;

	opaquePipeline.layout = newOpaqueLayout;
	transparentPipeline.layout = newTransparentLayout;

	VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
	vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertShaderStageInfo.module = meshVertexShader;
	vertShaderStageInfo.pName = "main";

	VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
	fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragShaderStageInfo.module = meshFragShader;
	fragShaderStageInfo.pName = "main";

	VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

	std::vector<VkDynamicState> dynamicStates = {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR
	};

	VkPipelineDynamicStateCreateInfo dynamicState{};
	dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
	dynamicState.pDynamicStates = dynamicStates.data();

	VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
	inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssembly.primitiveRestartEnable = VK_FALSE;

	VkPipelineRasterizationStateCreateInfo rasterizer{};
	rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer.depthClampEnable = VK_FALSE;
	rasterizer.rasterizerDiscardEnable = VK_FALSE;
	rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizer.lineWidth = 1.0f;
	rasterizer.cullMode = VK_CULL_MODE_NONE;
	rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
	rasterizer.depthBiasEnable = VK_FALSE;
	rasterizer.depthBiasConstantFactor = 0.0f;
	rasterizer.depthBiasClamp = 0.0f;
	rasterizer.depthBiasSlopeFactor = 0.0f;

	VkPipelineMultisampleStateCreateInfo multisampling{};
	multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.sampleShadingEnable = VK_FALSE;
	multisampling.rasterizationSamples = renderer.msaaSamples;

	VkPipelineColorBlendAttachmentState colorBlendAttachment{};
	colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	colorBlendAttachment.blendEnable = VK_FALSE;

	VkPipelineColorBlendStateCreateInfo colorBlending = {};
	colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlending.pNext = nullptr;
	colorBlending.logicOpEnable = VK_FALSE;
	colorBlending.logicOp = VK_LOGIC_OP_COPY;
	colorBlending.attachmentCount = 1;
	colorBlending.pAttachments = &colorBlendAttachment;

	VkPipelineDepthStencilStateCreateInfo depthStencil{};
	depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencil.depthTestEnable = VK_TRUE;
	depthStencil.depthWriteEnable = VK_TRUE;
	depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
	depthStencil.depthBoundsTestEnable = VK_FALSE;
	depthStencil.stencilTestEnable = VK_FALSE;
	depthStencil.front = {};
	depthStencil.back = {};
	depthStencil.minDepthBounds = 0.f;
	depthStencil.maxDepthBounds = 1.f;

	VkPipelineVertexInputStateCreateInfo vertexInputInfo = { .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };

	VkGraphicsPipelineCreateInfo pipelineInfo{};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineInfo.stageCount = 2;
	pipelineInfo.pStages = shaderStages;
	pipelineInfo.pVertexInputState = &vertexInputInfo;
	pipelineInfo.pInputAssemblyState = &inputAssembly;
	pipelineInfo.pViewportState = &viewportState;
	pipelineInfo.pRasterizationState = &rasterizer;
	pipelineInfo.pMultisampleState = &multisampling;
	pipelineInfo.pDepthStencilState = &depthStencil;
	pipelineInfo.pColorBlendState = &colorBlending;
	pipelineInfo.pDynamicState = &dynamicState;
	pipelineInfo.renderPass = renderer.renderPass;
	pipelineInfo.layout = newOpaqueLayout;

	VK_CHECK(vkCreateGraphicsPipelines(renderer.device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &opaquePipeline.pipeline));

	colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	colorBlendAttachment.blendEnable = VK_TRUE;
	colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
	colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

	depthStencil.depthTestEnable = VK_TRUE;
	depthStencil.depthWriteEnable =	VK_FALSE;
	depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
	depthStencil.depthBoundsTestEnable = VK_FALSE;
	depthStencil.stencilTestEnable = VK_FALSE;
	depthStencil.front = {};
	depthStencil.back = {};
	depthStencil.minDepthBounds = 0.f;
	depthStencil.maxDepthBounds = 1.f;

	pipelineInfo.layout = newTransparentLayout;

	VK_CHECK(vkCreateGraphicsPipelines(renderer.device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &transparentPipeline.pipeline));

	vkDestroyShaderModule(renderer.device, meshFragShader, nullptr);
	vkDestroyShaderModule(renderer.device, meshVertexShader, nullptr);
}

void GLTFMetallic_Roughness::clear_resources(VkDevice device)
{
	vkDestroyPipeline(device, opaquePipeline.pipeline, nullptr);
	vkDestroyPipelineLayout(device, opaquePipeline.layout, nullptr);

	vkDestroyPipeline(device, transparentPipeline.pipeline, nullptr);
	vkDestroyPipelineLayout(device, transparentPipeline.layout, nullptr);

	vkDestroyDescriptorSetLayout(device, materialLayout, nullptr);
}

MaterialInstance GLTFMetallic_Roughness::write_material(VkDevice device, MaterialPass pass, const MaterialResources& resources, DescriptorAllocatorGrowable& descriptorAllocator)
{
	MaterialInstance matData;
	matData.passType = pass;
	if (pass == MaterialPass::Transparent) {
		matData.pipeline = &transparentPipeline;
	}
	else {
		matData.pipeline = &opaquePipeline;
	}

	matData.materialSet = descriptorAllocator.allocate(device, materialLayout);

	writer.clear();
	writer.write_buffer(0, resources.dataBuffer, sizeof(MaterialConstants), resources.dataBufferOffset, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	writer.write_image(1, resources.colorImage.imageView, resources.colorSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	writer.write_image(2, resources.metalRoughImage.imageView, resources.metalRoughSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	writer.write_image(3, resources.normalImage.imageView, resources.normalSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	writer.write_image(4, resources.aoImage.imageView, resources.aoSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	writer.write_image(5, resources.depthImage.imageView, resources.depthSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);


	writer.update_set(device, matData.materialSet);

	return matData;
}

void MeshNode::draw(const glm::mat4& topMatrix, DrawContext &ctx)
{
	glm::mat4 nodeMatrix = topMatrix * worldTransform;

	for (auto& s : mesh->surfaces) {
		RenderObject def;
		def.indexCount = s.count;
		def.firstIndex = s.startIndex;
		def.indexBuffer = mesh->meshBuffers.indexBuffer.buffer;
		def.material = &s.material->data;
		def.shadowMaterial = &s.material->shadowcube;

		def.alphaCutoff = s.material->alphaCutoff;

		def.transform = nodeMatrix;
		def.vertexBufferAddress = mesh->meshBuffers.vertexBufferAddress;

		if (s.material->data.passType == MaterialPass::Transparent) {
			ctx.transparentSurfaces.push_back(def);
		} else {
			ctx.opaqueSurfaces.push_back(def);
		}
	}

	Node::draw(topMatrix, ctx);
}

};
