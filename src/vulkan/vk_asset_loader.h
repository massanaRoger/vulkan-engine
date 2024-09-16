#pragma once

#include "fastgltf/types.hpp"
#include "vulkan/vk_descriptors.h"
#include "vulkan/vk_renderer.h"
#include "vulkan/vk_types.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <vulkan/vulkan_core.h>

namespace Engine {

struct MeshAsset;
struct GLTFMaterial;

struct LoadedGLTF : public IRenderable {
	std::unordered_map<std::string, std::shared_ptr<MeshAsset>> meshes;
	std::unordered_map<std::string, std::shared_ptr<Node>> nodes;
	std::unordered_map<std::string, AllocatedImage> images;
	std::unordered_map<std::string, std::shared_ptr<GLTFMaterial>> materials;

	// nodes that dont have a parent, for iterating through the file in tree order
	std::vector<std::shared_ptr<Node>> topNodes;

	std::vector<VkSampler> samplers;

	DescriptorAllocatorGrowable descriptorPool;

	AllocatedBuffer materialDataBuffer;

	~LoadedGLTF() { clearAll(); };

	virtual void draw(const glm::mat4& topMatrix, DrawContext& ctx);

private:
	void clearAll();
};

std::optional<std::shared_ptr<LoadedGLTF>> loadGltf(std::string_view filePath);
std::optional<AllocatedImage> load_image(fastgltf::Asset& asset, fastgltf::Image& image, bool mipmapped);

}
