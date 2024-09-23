#include "vk_resource_manager.h"
#include "vulkan/vk_swapchain_manager.h"
#include <iostream>
#include <ostream>
#include <vulkan/vulkan_core.h>

namespace Engine {

void ResourceManager::create_allocator(VkDevice device, VkPhysicalDevice physicalDevice, VkInstance instance)
{
	VmaAllocatorCreateInfo allocatorInfo = {};
	allocatorInfo.physicalDevice = physicalDevice;
	allocatorInfo.device = device;
	allocatorInfo.instance = instance;
	allocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;

	vmaCreateAllocator(&allocatorInfo, &m_allocator);
}

AllocatedImage ResourceManager::create_image(VkDevice device, VkCommandPool commandPool, VkQueue queue, VkPhysicalDevice physicalDevice, void* data, VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped, const QueueFamilyIndices& familyIndices)
{
	size_t data_size = size.depth * size.width * size.height * 4;

	AllocatedBuffer uploadBuffer = create_buffer(data_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, VK_SHARING_MODE_EXCLUSIVE, familyIndices);

	void* dataRegion;
	vmaMapMemory(m_allocator, uploadBuffer.allocation, &dataRegion);
	memcpy(dataRegion, data, data_size);
	vmaUnmapMemory(m_allocator, uploadBuffer.allocation);

	int mipLevels = int(std::floor(std::log2(std::max(size.width, size.height)))) + 1;

	AllocatedImage newImage = create_image(device, size, format, usage | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, mipLevels);

	transition_image_layout(device, commandPool, queue, newImage.image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, mipLevels);

	copy_buffer_to_image(device, commandPool, queue, uploadBuffer.buffer, newImage.image, size.width, size.height);

	if (mipmapped) {
		generate_mipmaps(device, commandPool, queue, physicalDevice, newImage.image, VK_FORMAT_R8G8B8A8_SRGB, newImage.imageExtent.width, newImage.imageExtent.height, mipLevels);
	} else {
		transition_image_layout(device, commandPool, queue, newImage.image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1);
	}

	vmaDestroyBuffer(m_allocator, uploadBuffer.buffer, uploadBuffer.allocation);

	return newImage;
}

AllocatedBuffer ResourceManager::create_buffer(VkDeviceSize size, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage, VkSharingMode sharingMode, const QueueFamilyIndices& queueFamilies)
{
	VkBufferCreateInfo bufferInfo{};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = size;
	bufferInfo.usage = usage;
	bufferInfo.sharingMode = sharingMode;
	uint32_t indices[] = { queueFamilies.graphicsFamily.value(), queueFamilies.transferFamily.value() };
	if (bufferInfo.sharingMode == VK_SHARING_MODE_CONCURRENT) {
		bufferInfo.pQueueFamilyIndices = indices;
		bufferInfo.queueFamilyIndexCount = 2;
	}

	VmaAllocationCreateInfo vmaallocInfo = {};
	vmaallocInfo.usage = memoryUsage;
	vmaallocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

	AllocatedBuffer newBuffer;
	vmaCreateBuffer(m_allocator, &bufferInfo, &vmaallocInfo, &newBuffer.buffer, &newBuffer.allocation, &newBuffer.info);

	return newBuffer;
}

void ResourceManager::create_image(uint32_t width, uint32_t height, uint32_t mipLevels, VkSampleCountFlagBits numSamples, VkFormat format, VkImageTiling tiling, 
			    VkImageUsageFlags usage, VmaMemoryUsage memoryUsage, VkImage& image, VmaAllocation& imageAllocation)
{
	VkImageCreateInfo imageInfo{};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.extent.width = width;
	imageInfo.extent.height = height;
	imageInfo.extent.depth = 1;
	imageInfo.mipLevels = mipLevels;
	imageInfo.arrayLayers = 1;
	imageInfo.format = format;
	imageInfo.tiling = tiling;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageInfo.usage = usage;
	imageInfo.samples = numSamples;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VmaAllocationCreateInfo allocInfo{};
	allocInfo.usage = memoryUsage;

	if (vmaCreateImage(m_allocator, &imageInfo, &allocInfo, &image, &imageAllocation, nullptr) != VK_SUCCESS) {
		std::cerr << "failed to create image with VMA!" << std::endl;
		abort();
	}
}

AllocatedImage ResourceManager::create_image(VkDevice device, VkExtent3D size, VkFormat format, VkImageUsageFlags usage, uint32_t mipLevels = 1)
{
	AllocatedImage newImage;
	newImage.imageFormat = format;
	newImage.imageExtent = size;

	VkImageCreateInfo imageInfo = {};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.pNext = nullptr;

	imageInfo.imageType = VK_IMAGE_TYPE_2D;

	imageInfo.format = format;
	imageInfo.extent = size;

	imageInfo.mipLevels = 1;
	imageInfo.arrayLayers = 1;

	// MSAA. we will not be using it by default, so default it to 1 sample per pixel.
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;

	// Optimal tiling, which means the image is stored on the best gpu format
	imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageInfo.usage = usage;

	imageInfo.mipLevels = mipLevels;

	// always allocate images on dedicated GPU memory
	VmaAllocationCreateInfo allocinfo = {};
	allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
	allocinfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	// allocate and create the image
	VK_CHECK(vmaCreateImage(m_allocator, &imageInfo, &allocinfo, &newImage.image, &newImage.allocation, nullptr));

	// if the format is a depth format, we will need to have it use the correct
	// aspect flag
	VkImageAspectFlags aspectFlag = VK_IMAGE_ASPECT_COLOR_BIT;
	if (format == VK_FORMAT_D32_SFLOAT) {
		aspectFlag = VK_IMAGE_ASPECT_DEPTH_BIT;
	}

	// build a image-view for the image
	VkImageViewCreateInfo imageViewInfo = {};
	imageViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	imageViewInfo.pNext = nullptr;

	imageViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	imageViewInfo.image = newImage.image;
	imageViewInfo.format = format;
	imageViewInfo.subresourceRange.baseMipLevel = 0;
	imageViewInfo.subresourceRange.levelCount = 1;
	imageViewInfo.subresourceRange.baseArrayLayer = 0;
	imageViewInfo.subresourceRange.layerCount = 1;
	imageViewInfo.subresourceRange.aspectMask = aspectFlag;

	imageViewInfo.subresourceRange.levelCount = imageInfo.mipLevels;

	VK_CHECK(vkCreateImageView(device, &imageViewInfo, nullptr, &newImage.imageView));


	return newImage;
}

void ResourceManager::transition_image_layout(VkDevice device, VkCommandPool commandPool, VkQueue queue, VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t mipLevels)
{
	VkCommandBuffer commandBuffer = begin_single_time_commands(device, commandPool);

	VkImageMemoryBarrier barrier{};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.oldLayout = oldLayout;
	barrier.newLayout = newLayout;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = image;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = mipLevels;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;

	VkPipelineStageFlags sourceStage;
	VkPipelineStageFlags destinationStage;

	if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

		sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
	} else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	} else if (oldLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
		barrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		sourceStage = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
		destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	} else {
		std::cerr << "Unsupported layout transition!" << std::endl;
		abort();
	}

	vkCmdPipelineBarrier(
		commandBuffer,
		sourceStage, destinationStage,
		0,
		0, nullptr,
		0, nullptr,
		1, &barrier
	);

	end_single_time_commands(device, commandBuffer, queue, commandPool);
}

void ResourceManager::copy_buffer_to_image(VkDevice device, VkCommandPool commandPool, VkQueue queue, VkBuffer buffer, VkImage image, uint32_t width, uint32_t height) {
	VkCommandBuffer commandBuffer = begin_single_time_commands(device, commandPool);

	VkBufferImageCopy region{};
	region.bufferOffset = 0;
	region.bufferRowLength = 0;
	region.bufferImageHeight = 0;

	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.mipLevel = 0;
	region.imageSubresource.baseArrayLayer = 0;
	region.imageSubresource.layerCount = 1;

	region.imageOffset = {0, 0, 0};
	region.imageExtent = {
		width,
		height,
		1
	};

	vkCmdCopyBufferToImage(commandBuffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

	end_single_time_commands(device, commandBuffer, queue, commandPool);
}

void ResourceManager::copy_buffer(VkDevice device, VkCommandPool commandPool, VkQueue queue, VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size)
{
	VkCommandBuffer commandBuffer = begin_single_time_commands(device, commandPool);

	VkBufferCopy copyRegion{};
	copyRegion.size = size;
	vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

	end_single_time_commands(device, commandBuffer, queue, commandPool);
}

void ResourceManager::generate_mipmaps(VkDevice device, VkCommandPool commandPool, VkQueue queue, VkPhysicalDevice physicalDevice, VkImage image, VkFormat imageFormat, int32_t texWidth, int32_t texHeight, uint32_t mipLevels)
{
	// Check if image format supports linear blitting
	VkFormatProperties formatProperties;
	vkGetPhysicalDeviceFormatProperties(physicalDevice, imageFormat, &formatProperties);

	if (!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)) {
		std::cerr << "Texture image format does not support lienar blitting!" << std::endl;
		abort();
	}

	VkCommandBuffer commandBuffer = begin_single_time_commands(device, commandPool);

	VkImageMemoryBarrier barrier{};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.image = image;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;
	barrier.subresourceRange.levelCount = 1;

	int32_t mipWidth = texWidth;
	int32_t mipHeight = texHeight;

	for (uint32_t i = 1; i < mipLevels; i++) {
		barrier.subresourceRange.baseMipLevel = i - 1;
		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

		vkCmdPipelineBarrier(commandBuffer,
		       VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
		       0, nullptr,
		       0, nullptr,
		       1, &barrier);

		VkImageBlit blit{};
		blit.srcOffsets[0] = { 0, 0, 0 };
		blit.srcOffsets[1] = { mipWidth, mipHeight, 1 };
		blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		blit.srcSubresource.mipLevel = i - 1;
		blit.srcSubresource.baseArrayLayer = 0;
		blit.srcSubresource.layerCount = 1;
		blit.dstOffsets[0] = { 0, 0, 0 };
		blit.dstOffsets[1] = { mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1 };
		blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		blit.dstSubresource.mipLevel = i;
		blit.dstSubresource.baseArrayLayer = 0;
		blit.dstSubresource.layerCount = 1;

		vkCmdBlitImage(commandBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR);

		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		vkCmdPipelineBarrier(commandBuffer,
		       VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
		       0, nullptr,
		       0, nullptr,
		       1, &barrier);

		if (mipWidth > 1) mipWidth /= 2;
		if (mipHeight > 1) mipHeight /= 2;
	}

	barrier.subresourceRange.baseMipLevel = mipLevels - 1;
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        vkCmdPipelineBarrier(commandBuffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
            0, nullptr,
            0, nullptr,
            1, &barrier);

	end_single_time_commands(device, commandBuffer, queue, commandPool);
}

VmaAllocator ResourceManager::get_allocator() const
{
	return m_allocator;
}

VkCommandBuffer ResourceManager::begin_single_time_commands(VkDevice device, VkCommandPool commandPool) {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = commandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    return commandBuffer;
}

void ResourceManager::end_single_time_commands(VkDevice device, VkCommandBuffer commandBuffer, VkQueue queue, VkCommandPool commandPool) {
    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(queue);

    vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
}


}
