#include "vk_renderer.h"
#include "SDL_stdinc.h"
#include "SDL_video.h"
#include "vk_types.h"
#include "vk_utils.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <set>
#include <stdlib.h>
#include <vector>
#include <vulkan/vulkan_core.h>
#include <SDL_vulkan.h>

void Engine::Renderer::init_vulkan(SDL_Window* window)
{
	m_window = window;
	create_instance();
	setup_debug_messenger();
	create_surface();
	pick_physical_device();
	create_logical_device();
	create_swap_chain();
}

void Engine::Renderer::create_instance()
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

void Engine::Renderer::cleanup()
{
	vkDestroySwapchainKHR(m_device, m_swapChain, nullptr);
	vkDestroyDevice(m_device, nullptr);
	if (c_enableValidationLayers) {
		destroy_debug_utils_messenger_EXT(m_instance, m_debugMessenger, nullptr);
	}
	vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
	vkDestroyInstance(m_instance, nullptr);
}

bool Engine::Renderer::check_validation_layer_support()
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

void Engine::Renderer::setup_debug_messenger() {
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

VKAPI_ATTR VkBool32 VKAPI_CALL Engine::Renderer::debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData) {

    std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;

    return VK_FALSE;
}

void Engine::Renderer::populate_debug_messenger_create_info(VkDebugUtilsMessengerCreateInfoEXT& createInfo) {
    createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    createInfo.pfnUserCallback = debugCallback;
}

void Engine::Renderer::pick_physical_device()
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
		VkPhysicalDeviceFeatures2 deviceFeatures{};
		deviceFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
		vkGetPhysicalDeviceProperties2(device, &deviceProperties);
		vkGetPhysicalDeviceFeatures2(device, &deviceFeatures);

		// Optional will have value if at least it has one queue family for displaying graphics
		QueueFamilyIndices indices = findQueueFamilies(device);

		// If has vulkan support and geometry shader and has a graphics queue family
		if (deviceProperties.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU
			&& deviceFeatures.features.geometryShader
			&& indices.isComplete()
			// Check if device supports the required extensions
			&& check_device_extension_support(device)
		) {
			// Check swapchain support
			SwapChainSupportDetails swapChainSupport = query_swap_chain_support(device);
			if (!swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty()) {
				m_physicalDevice = device;
			}
		}
	}

	if (m_physicalDevice == VK_NULL_HANDLE) {
		std::cerr << "No suitable GPU found" << std::endl;
		abort();
	}
}

Engine::QueueFamilyIndices Engine::Renderer::findQueueFamilies(VkPhysicalDevice device)
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
			indices.graphicsFamily = i;
		}

		VkBool32 presentSupport = false;
		vkGetPhysicalDeviceSurfaceSupportKHR(device, i, m_surface, &presentSupport);
		if (presentSupport) {
			indices.presentFamily = i;
		}

		if (indices.isComplete()) {
			break;
		}

		i++;
	}

	return indices;
}

void Engine::Renderer::create_logical_device()
{
	QueueFamilyIndices indices = findQueueFamilies(m_physicalDevice);

	std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
	std::set<uint32_t> uniqueQueueFamilies = {indices.graphicsFamily.value(), indices.presentFamily.value()};

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

	VkDeviceCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
	createInfo.pQueueCreateInfos = queueCreateInfos.data();
	createInfo.pEnabledFeatures = &deviceFeatures;

	// Enable extensions
	createInfo.enabledExtensionCount = static_cast<uint32_t>(c_deviceExtensions.size());
	createInfo.ppEnabledExtensionNames = c_deviceExtensions.data();

	if (c_enableValidationLayers) {
		createInfo.enabledLayerCount = static_cast<uint32_t>(c_validationLayers.size());
		createInfo.ppEnabledLayerNames = c_validationLayers.data();
	} else {
		createInfo.enabledLayerCount = 0;
	}

	VK_CHECK(vkCreateDevice(m_physicalDevice, &createInfo, nullptr, &m_device));

	vkGetDeviceQueue(m_device, indices.graphicsFamily.value(), 0, &m_graphicsQueue);
	vkGetDeviceQueue(m_device, indices.presentFamily.value(), 0, &m_presentQueue);
}

void Engine::Renderer::create_surface()
{
	if (SDL_Vulkan_CreateSurface(m_window, m_instance, &m_surface) == SDL_FALSE) {
		std::cerr << "Failed to create window surface" << std::endl;
		abort();
	}
}

bool Engine::Renderer::check_device_extension_support(VkPhysicalDevice device)
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

Engine::SwapChainSupportDetails Engine::Renderer::query_swap_chain_support(VkPhysicalDevice device)
{
	SwapChainSupportDetails details;

	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, m_surface, &details.capabilities);

	uint32_t formatCount = 0;
	vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface, &formatCount, nullptr);

	if (formatCount != 0) {
		details.formats.resize(formatCount);
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface, &formatCount, details.formats.data());
	}

	uint32_t presentModeCount = 0;
	vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_surface, &presentModeCount, nullptr);
	if (presentModeCount != 0) {
		details.presentModes.resize(presentModeCount);
		vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_surface, &presentModeCount, details.presentModes.data());
	}

	return details;
}


VkSurfaceFormatKHR Engine::Renderer::choose_swap_chain_format(const std::vector<VkSurfaceFormatKHR>& availableFormats)
{
	for (const auto& availableFormat : availableFormats) {
		if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
			return availableFormat;
		}
	}
	
	return availableFormats[0];
}


VkPresentModeKHR Engine::Renderer::choose_swap_present_mode(const std::vector<VkPresentModeKHR>& availablePresentModes)
{
	for (const auto& availablePresentMode : availablePresentModes) {
		if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
			return availablePresentMode;
		}
	}

	return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D Engine::Renderer::choose_swap_extent(const VkSurfaceCapabilitiesKHR& capabilities)
{
	if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
		return capabilities.currentExtent;
	} else {
		int width, height;
		SDL_Vulkan_GetDrawableSize(m_window, &width, &height);

		VkExtent2D actualExtent = {
			static_cast<uint32_t>(width),
			static_cast<uint32_t>(height),
		};

		actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
		actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

		return actualExtent;
	}
}

void Engine::Renderer::create_swap_chain()
{
	Engine::SwapChainSupportDetails swapChainSupport = query_swap_chain_support(m_physicalDevice);

	VkSurfaceFormatKHR surfaceFormat = choose_swap_chain_format(swapChainSupport.formats);
	VkPresentModeKHR presentMode = choose_swap_present_mode(swapChainSupport.presentModes);
	VkExtent2D extent = choose_swap_extent(swapChainSupport.capabilities);

	uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
	if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
		imageCount = swapChainSupport.capabilities.maxImageCount;
	}

	VkSwapchainCreateInfoKHR createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	createInfo.surface = m_surface;

	createInfo.minImageCount = imageCount;
	createInfo.imageFormat = surfaceFormat.format;
	createInfo.imageColorSpace = surfaceFormat.colorSpace;
	createInfo.imageExtent = extent;
	createInfo.imageArrayLayers = 1;
	createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

	m_swapChainImageFormat = surfaceFormat.format;
	m_swapChainExtent = extent;

	QueueFamilyIndices indices = findQueueFamilies(m_physicalDevice);
	uint32_t queueFamilyIndices[] = {indices.graphicsFamily.value(), indices.presentFamily.value()};

	// If the graphics queue family and presentation queue family are not the same
	if (indices.graphicsFamily != indices.presentFamily) {
		createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		createInfo.queueFamilyIndexCount = 2;
		createInfo.pQueueFamilyIndices = queueFamilyIndices;
	} else {
		createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		createInfo.queueFamilyIndexCount = 0;
		createInfo.pQueueFamilyIndices = nullptr;
	}

	createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
	createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	createInfo.presentMode = presentMode;
	createInfo.clipped = VK_TRUE;
	createInfo.oldSwapchain = VK_NULL_HANDLE;

	VK_CHECK(vkCreateSwapchainKHR(m_device, &createInfo, nullptr, &m_swapChain));

	vkGetSwapchainImagesKHR(m_device, m_swapChain, &imageCount, nullptr);
	m_swapChainImages.resize(imageCount);
	vkGetSwapchainImagesKHR(m_device, m_swapChain, &imageCount, m_swapChainImages.data());
}
