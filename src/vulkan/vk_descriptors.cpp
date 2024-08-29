#include "vk_descriptors.h"
#include "vulkan/vk_types.h"
#include <vulkan/vulkan_core.h>

namespace Engine {

VkDescriptorPool DescriptorAllocatorGrowable::get_pool(VkDevice device)
{
	VkDescriptorPool newPool;
	if (m_readyPools.size() != 0) {
		newPool = m_readyPools.back();
		m_readyPools.pop_back();
	}
	else {
		//need to create a new pool
		newPool = create_pool(device, m_setsPerPool, m_ratios);

		m_setsPerPool = m_setsPerPool * 1.5;
		if (m_setsPerPool > 4092) {
			m_setsPerPool = 4092;
		}
	}   

	return newPool;
}

VkDescriptorPool DescriptorAllocatorGrowable::create_pool(VkDevice device, uint32_t setCount, std::span<PoolSizeRatio> poolRatios)
{
	std::vector<VkDescriptorPoolSize> poolSizes;
	for (PoolSizeRatio ratio : poolRatios) {
		poolSizes.emplace_back(VkDescriptorPoolSize{
			.type = ratio.type,
			.descriptorCount = uint32_t(ratio.ratio * setCount)
		});
	}

	VkDescriptorPoolCreateInfo pool_info = {};
	pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	pool_info.flags = 0;
	pool_info.maxSets = setCount;
	pool_info.poolSizeCount = (uint32_t)poolSizes.size();
	pool_info.pPoolSizes = poolSizes.data();

	VkDescriptorPool newPool;
	vkCreateDescriptorPool(device, &pool_info, nullptr, &newPool);
	return newPool;
}

void DescriptorAllocatorGrowable::init(VkDevice device, uint32_t maxSets, std::span<PoolSizeRatio> poolRatios)
{
	m_ratios.clear();

	for (auto r : poolRatios) {
		m_ratios.push_back(r);
	}

	VkDescriptorPool newPool = create_pool(device, maxSets, poolRatios);

	m_setsPerPool = maxSets * 1.5; //grow it next allocation

	m_readyPools.push_back(newPool);
}

void DescriptorAllocatorGrowable::clear_pools(VkDevice device)
{ 
	for (auto p : m_readyPools) {
		vkResetDescriptorPool(device, p, 0);
	}
	for (auto p : m_fullPools) {
		vkResetDescriptorPool(device, p, 0);
		m_readyPools.push_back(p);
	}
	m_fullPools.clear();
}

void DescriptorAllocatorGrowable::destroy_pools(VkDevice device)
{
	for (auto p : m_readyPools) {
		vkDestroyDescriptorPool(device, p, nullptr);
	}
	m_readyPools.clear();
	for (auto p : m_fullPools) {
		vkDestroyDescriptorPool(device,p,nullptr);
	}
	m_fullPools.clear();
}

VkDescriptorSet DescriptorAllocatorGrowable::allocate(VkDevice device, VkDescriptorSetLayout layout, void* pNext)
{
	//get or create a pool to allocate from
	VkDescriptorPool poolToUse = get_pool(device);

	VkDescriptorSetAllocateInfo allocInfo = {};
	allocInfo.pNext = pNext;
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = poolToUse;
	allocInfo.descriptorSetCount = 1;
	allocInfo.pSetLayouts = &layout;

	VkDescriptorSet ds;
	VkResult result = vkAllocateDescriptorSets(device, &allocInfo, &ds);

	//allocation failed. Try again
	if (result == VK_ERROR_OUT_OF_POOL_MEMORY || result == VK_ERROR_FRAGMENTED_POOL) {

		m_fullPools.push_back(poolToUse);

		poolToUse = get_pool(device);
		allocInfo.descriptorPool = poolToUse;

		VK_CHECK(vkAllocateDescriptorSets(device, &allocInfo, &ds));
	}

	m_readyPools.push_back(poolToUse);
	return ds;
}

void DescriptorWriter::write_buffer(int binding, VkBuffer buffer, size_t size, size_t offset, VkDescriptorType type)
{
	VkDescriptorBufferInfo& info = m_bufferInfos.emplace_back(VkDescriptorBufferInfo{
		.buffer = buffer,
		.offset = offset,
		.range = size
		});

	VkWriteDescriptorSet write = {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};

	write.dstBinding = binding;
	write.dstSet = VK_NULL_HANDLE; //left empty for now until we need to write it
	write.descriptorCount = 1;
	write.descriptorType = type;
	write.pBufferInfo = &info;

	m_writes.push_back(write);
}

void DescriptorWriter::write_image(int binding,VkImageView image, VkSampler sampler,  VkImageLayout layout, VkDescriptorType type)
{
	VkDescriptorImageInfo& info = m_imageInfos.emplace_back(VkDescriptorImageInfo{
		.sampler = sampler,
		.imageView = image,
		.imageLayout = layout
	});

	VkWriteDescriptorSet write = { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };

	write.dstBinding = binding;
	write.dstSet = VK_NULL_HANDLE; //left empty for now until we need to write it
	write.descriptorCount = 1;
	write.descriptorType = type;
	write.pImageInfo = &info;

	m_writes.push_back(write);
}

void DescriptorWriter::clear()
{
    m_imageInfos.clear();
    m_writes.clear();
    m_bufferInfos.clear();
}

void DescriptorWriter::update_set(VkDevice device, VkDescriptorSet set)
{
    for (VkWriteDescriptorSet& write : m_writes) {
        write.dstSet = set;
    }

    vkUpdateDescriptorSets(device, (uint32_t)m_writes.size(), m_writes.data(), 0, nullptr);
}

}
