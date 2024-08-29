#pragma once

#include <deque>
#include <span>
#include <vulkan/vulkan_core.h>

namespace Engine {
class DescriptorAllocatorGrowable {
public:
	struct PoolSizeRatio {
		VkDescriptorType type;
		float ratio;
	};

	void init(VkDevice device, uint32_t initialSets, std::span<PoolSizeRatio> poolRatios);
	void clear_pools(VkDevice device);
	void destroy_pools(VkDevice device);

	VkDescriptorSet allocate(VkDevice device, VkDescriptorSetLayout layout, void* pNext = nullptr);
private:
	VkDescriptorPool get_pool(VkDevice device);
	VkDescriptorPool create_pool(VkDevice device, uint32_t setCount, std::span<PoolSizeRatio> poolRatios);

	std::vector<PoolSizeRatio> m_ratios;
	std::vector<VkDescriptorPool> m_fullPools;
	std::vector<VkDescriptorPool> m_readyPools;
	uint32_t m_setsPerPool;
};

class DescriptorWriter {

public:
	void write_image(int binding,VkImageView image,VkSampler sampler , VkImageLayout layout, VkDescriptorType type);
	void write_buffer(int binding,VkBuffer buffer,size_t size, size_t offset,VkDescriptorType type); 

	void clear();
	void update_set(VkDevice device, VkDescriptorSet set);

private:
	std::deque<VkDescriptorImageInfo> m_imageInfos;
	std::deque<VkDescriptorBufferInfo> m_bufferInfos;
	std::vector<VkWriteDescriptorSet> m_writes;
};
}
