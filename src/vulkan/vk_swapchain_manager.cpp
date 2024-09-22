#include "vk_swapchain_manager.h"
#include "SDL_events.h"
#include "SDL_video.h"
#include "SDL_vulkan.h"
#include "vulkan/vk_renderer.h"
#include "vulkan/vk_types.h"
#include <vulkan/vulkan_core.h>

namespace Engine {
	
	
void SwapchainManager::create_swapchain(VkDevice device, VkSurfaceKHR surface, VkPhysicalDevice physicalDevice, const QueueFamilyIndices& queueIndices, SDL_Window* window)
{
	SwapchainSupportDetails swapChainSupport = query_swapchain_support(physicalDevice, surface);

	VkSurfaceFormatKHR surfaceFormat = choose_swapchain_format(swapChainSupport.formats);
	VkPresentModeKHR presentMode = choose_swapchain_present_mode(swapChainSupport.presentModes);
	VkExtent2D extent = choose_swapchain_extent(swapChainSupport.capabilities, window);

	uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
	if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
		imageCount = swapChainSupport.capabilities.maxImageCount;
	}

	VkSwapchainCreateInfoKHR createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	createInfo.surface = surface;

	createInfo.minImageCount = imageCount;
	createInfo.imageFormat = surfaceFormat.format;
	createInfo.imageColorSpace = surfaceFormat.colorSpace;
	createInfo.imageExtent = extent;
	createInfo.imageArrayLayers = 1;
	createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

	m_swapchainImageFormat = surfaceFormat.format;
	m_swapchainExtent = extent;

	uint32_t queueFamilyIndices[] = {queueIndices.graphicsFamily.value(), queueIndices.presentFamily.value()};

	// If the graphics queue family and presentation queue family are not the same
	if (queueIndices.graphicsFamily != queueIndices.presentFamily) {
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

	VK_CHECK(vkCreateSwapchainKHR(device, &createInfo, nullptr, &m_swapchain));

	vkGetSwapchainImagesKHR(device, m_swapchain, &imageCount, nullptr);
	m_swapchainImages.resize(imageCount);
	vkGetSwapchainImagesKHR(device, m_swapchain, &imageCount, m_swapchainImages.data());
}

void SwapchainManager::create_swapchain_frame_buffers(VkDevice device, VkRenderPass renderPass)
{

	m_swapchainFramebuffers.resize(m_swapchainImageViews.size());
	for (size_t i = 0; i < m_swapchainFramebuffers.size(); i++) {
		std::array<VkImageView, 3> attachments = {
			m_drawImage.imageView,
			m_depthImage.imageView,
			m_swapchainImageViews[i],
		};

		VkFramebufferCreateInfo framebufferInfo = {};
		framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferInfo.renderPass = renderPass;
		framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
		framebufferInfo.pAttachments = attachments.data();
		framebufferInfo.width = m_swapchainExtent.width;
		framebufferInfo.height = m_swapchainExtent.height;
		framebufferInfo.layers = 1;

		VK_CHECK(vkCreateFramebuffer(device, &framebufferInfo, nullptr, &m_swapchainFramebuffers[i]));
	}
}

void SwapchainManager::create_depth_images(VkDevice device, VkFormat depthFormat, VkSampleCountFlagBits samples, VmaAllocator allocator)
{
	Renderer::create_image(m_swapchainExtent.width, m_swapchainExtent.height, 1, samples, depthFormat, VK_IMAGE_TILING_OPTIMAL, 
	      VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VMA_MEMORY_USAGE_GPU_ONLY, m_depthImage.image, m_depthImage.allocation, allocator);

	VkImageViewCreateInfo viewInfo{};
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.image = m_depthImage.image;
	viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	viewInfo.format = depthFormat;
	viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
	viewInfo.subresourceRange.baseMipLevel = 0;
	viewInfo.subresourceRange.levelCount = 1;
	viewInfo.subresourceRange.baseArrayLayer = 0;
	viewInfo.subresourceRange.layerCount = 1;

	vkCreateImageView(device, &viewInfo, nullptr, &m_depthImage.imageView);
}

void SwapchainManager::create_color_images(VkDevice device, VkSampleCountFlagBits samples, VmaAllocator allocator)
{
	VkFormat colorFormat = m_swapchainImageFormat;

	Renderer::create_image(m_swapchainExtent.width, m_swapchainExtent.height, 1, samples, colorFormat,
	      VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
	      VMA_MEMORY_USAGE_GPU_ONLY, m_drawImage.image, m_drawImage.allocation, allocator);

	VkImageViewCreateInfo viewInfo{};
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.image = m_drawImage.image;
	viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	viewInfo.format = colorFormat;
	viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	viewInfo.subresourceRange.baseMipLevel = 0;
	viewInfo.subresourceRange.levelCount = 1;
	viewInfo.subresourceRange.baseArrayLayer = 0;
	viewInfo.subresourceRange.layerCount = 1;
	vkCreateImageView(device, &viewInfo, nullptr, &m_drawImage.imageView);

}

VkSwapchainKHR SwapchainManager::get_swapchain() const
{
	return m_swapchain;
}

VkFormat SwapchainManager::get_swapchain_image_format() const
{
	return m_swapchainImageFormat;
}

VkExtent2D SwapchainManager::get_swapchain_extent() const
{
	return m_swapchainExtent;
}

VkFramebuffer SwapchainManager::get_swapchain_framebuffer(uint32_t index) const
{
	return m_swapchainFramebuffers[index];
}

void SwapchainManager::cleanup_swapchain(VkDevice device, VmaAllocator allocator)
{
	vkDestroyImageView(device, m_depthImage.imageView, nullptr);
	vmaDestroyImage(allocator, m_depthImage.image, m_depthImage.allocation);

	vkDestroyImageView(device, m_drawImage.imageView, nullptr);
	vmaDestroyImage(allocator, m_drawImage.image, m_drawImage.allocation);

	for (size_t i = 0; i < m_swapchainFramebuffers.size(); i++) {
		vkDestroyFramebuffer(device, m_swapchainFramebuffers[i], nullptr);
	}

	for (size_t i = 0; i < m_swapchainImageViews.size(); i++) {
		vkDestroyImageView(device, m_swapchainImageViews[i], nullptr);
	}

	vkDestroySwapchainKHR(device, m_swapchain, nullptr);
}

SwapchainSupportDetails SwapchainManager::query_swapchain_support(VkPhysicalDevice device, VkSurfaceKHR surface)
{

	SwapchainSupportDetails details;

	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

	uint32_t formatCount = 0;
	vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);

	if (formatCount != 0) {
		details.formats.resize(formatCount);
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
	}

	uint32_t presentModeCount = 0;
	vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);
	if (presentModeCount != 0) {
		details.presentModes.resize(presentModeCount);
		vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());
	}

	return details;
}

void SwapchainManager::create_swapchain_image_views(VkDevice device)
{

	m_swapchainImageViews.resize(m_swapchainImages.size());

	for (size_t i = 0; i < m_swapchainImages.size(); i++) {
		VkImageViewCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		createInfo.image = m_swapchainImages[i];
		createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		createInfo.format = m_swapchainImageFormat;

		createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

		createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		createInfo.subresourceRange.baseMipLevel = 0;
		createInfo.subresourceRange.levelCount = 1;
		createInfo.subresourceRange.baseArrayLayer = 0;
		createInfo.subresourceRange.layerCount = 1;

		VK_CHECK(vkCreateImageView(device, &createInfo, nullptr, &m_swapchainImageViews[i]));

	}

}

void SwapchainManager::recreate_swapchain(VkDevice device, SDL_Window* window, VkSurfaceKHR surface, VkPhysicalDevice physicalDevice, const QueueFamilyIndices& queueIndices, VkRenderPass renderPass, VkSampleCountFlagBits colorSamples, VkSampleCountFlagBits depthSamples, VkFormat depthFormat, VmaAllocator allocator)
{
	bool isMinimized = SDL_GetWindowFlags(window) & SDL_WINDOW_MINIMIZED;

	while (isMinimized) {
		isMinimized = SDL_GetWindowFlags(window) & SDL_WINDOW_MINIMIZED;
		SDL_WaitEvent(nullptr);
	}

	vkDeviceWaitIdle(device);

	cleanup_swapchain(device, allocator);

	create_swapchain(device, surface, physicalDevice, queueIndices, window);

	create_swapchain_image_views(device);
	create_swapchain_frame_buffers(device, renderPass);
	create_color_images(device, colorSamples, allocator);
	create_depth_images(device, depthFormat, depthSamples, allocator);
}



VkSurfaceFormatKHR SwapchainManager::choose_swapchain_format(const std::vector<VkSurfaceFormatKHR>& availableFormats)
{
	for (const auto& availableFormat : availableFormats) {
		if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
			return availableFormat;
		}
	}
	
	return availableFormats[0];
}

VkPresentModeKHR SwapchainManager::choose_swapchain_present_mode(const std::vector<VkPresentModeKHR>& availablePresentModes)
{
	for (const auto& availablePresentMode : availablePresentModes) {
		if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
			return availablePresentMode;
		}
	}

	return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D SwapchainManager::choose_swapchain_extent(const VkSurfaceCapabilitiesKHR& capabilities, SDL_Window* window)
{
	if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
		return capabilities.currentExtent;
	} else {
		int width, height;
		SDL_Vulkan_GetDrawableSize(window, &width, &height);

		VkExtent2D actualExtent = {
			static_cast<uint32_t>(width),
			static_cast<uint32_t>(height),
		};

		actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
		actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

		return actualExtent;
	}
}


}
