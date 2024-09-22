#pragma once

#include "SDL_video.h"
#include "vulkan/vk_types.h"
#include <vulkan/vulkan_core.h>
namespace Engine {

struct QueueFamilyIndices {
	std::optional<uint32_t> graphicsFamily;
	std::optional<uint32_t> presentFamily;
	std::optional<uint32_t> transferFamily;

	bool isComplete() {
		return graphicsFamily.has_value() && presentFamily.has_value() && transferFamily.has_value();
	}
};

struct SwapchainSupportDetails {
	VkSurfaceCapabilitiesKHR capabilities;
	std::vector<VkSurfaceFormatKHR> formats;
	std::vector<VkPresentModeKHR> presentModes;
};

class SwapchainManager {

public:

	void create_swapchain(VkDevice device, VkSurfaceKHR surface, VkPhysicalDevice physicalDevice, const QueueFamilyIndices& queueIndices, SDL_Window* window);
	void recreate_swapchain(VkDevice device, SDL_Window* window, VkSurfaceKHR surface, VkPhysicalDevice physicalDevice, const QueueFamilyIndices& queueIndices, VkRenderPass renderPass, VkSampleCountFlagBits colorSamples, VkSampleCountFlagBits depthSamples, VkFormat depthFormat, VmaAllocator allocator);
	void cleanup_swapchain(VkDevice device, VmaAllocator allocator);

	SwapchainSupportDetails query_swapchain_support(VkPhysicalDevice device, VkSurfaceKHR surface);

	void create_swapchain_image_views(VkDevice device);
	void create_swapchain_frame_buffers(VkDevice device, VkRenderPass renderPass);

	void create_depth_images(VkDevice device, VkFormat depthFormat, VkSampleCountFlagBits samples, VmaAllocator allocator);
	void create_color_images(VkDevice device, VkSampleCountFlagBits samples, VmaAllocator allocator);

	[[nodiscard]] VkSwapchainKHR get_swapchain() const;
	[[nodiscard]] VkFormat get_swapchain_image_format() const;
	[[nodiscard]] VkExtent2D get_swapchain_extent() const;
	[[nodiscard]] VkFramebuffer get_swapchain_framebuffer(uint32_t index) const;

private:
	VkSwapchainKHR m_swapchain;
	std::vector<VkImage> m_swapchainImages;
	VkFormat m_swapchainImageFormat;
	std::vector<VkImageView> m_swapchainImageViews;
	std::vector<VkFramebuffer> m_swapchainFramebuffers;
	VkExtent2D m_swapchainExtent;

	AllocatedImage m_drawImage;
	AllocatedImage m_depthImage;

	VkSurfaceFormatKHR choose_swapchain_format(const std::vector<VkSurfaceFormatKHR>& availableFormats);
	VkPresentModeKHR choose_swapchain_present_mode(const std::vector<VkPresentModeKHR>& availablePresentModes);
	VkExtent2D choose_swapchain_extent(const VkSurfaceCapabilitiesKHR& capabilities, SDL_Window* window);
};

}
