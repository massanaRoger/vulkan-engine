#pragma once

#include "vulkan/vk_swapchain_manager.h"
#include "vulkan/vk_types.h"
namespace Engine {

class ResourceManager {
public:
	void create_allocator(VkDevice device, VkPhysicalDevice physicalDevice, VkInstance instance);
	
	AllocatedBuffer create_buffer(VkDeviceSize size, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage, VkSharingMode sharingMode, const QueueFamilyIndices& queueFamilies);

	AllocatedImage create_image(VkDevice device, VkCommandPool commandPool, VkQueue queue, VkPhysicalDevice physicalDevice, void* data, VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped, const QueueFamilyIndices& familyIndices);
	void create_image(uint32_t width, uint32_t height, uint32_t mipLevels, VkSampleCountFlagBits numSamples, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VmaMemoryUsage memoryUsage, VkImage& image, VmaAllocation& imageAllocation, VkImageCreateFlags flags = 0, uint32_t arrayLayers = 1);
	AllocatedImage create_image(VkDevice device, VkExtent3D size, VkFormat format, VkImageUsageFlags usage, uint32_t mipLevels);

	void transition_image_layout(VkDevice device, VkCommandPool commandPool, VkQueue queue, VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t mipLevels, uint32_t layers = 1, VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_COLOR_BIT);
	void copy_buffer_to_image(VkDevice device, VkCommandPool commandPool, VkQueue queue, VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);
	void copy_buffer(VkDevice device, VkCommandPool commandPool, VkQueue queue, VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
	void generate_mipmaps(VkDevice device, VkCommandPool commandPool, VkQueue queue, VkPhysicalDevice physicalDevice, VkImage image, VkFormat imageFormat, int32_t texWidth, int32_t texHeight, uint32_t mipLevels);

	[[nodiscard]] VmaAllocator get_allocator() const;

	void destroy_image(VkDevice device, const AllocatedImage& img);
	void destroy_buffer(const AllocatedBuffer& buffer);


private:

	VmaAllocator m_allocator;

	void end_single_time_commands(VkDevice device, VkCommandBuffer commandBuffer, VkQueue queue, VkCommandPool commandPool);
	VkCommandBuffer begin_single_time_commands(VkDevice device, VkCommandPool commandPool);

};

}
