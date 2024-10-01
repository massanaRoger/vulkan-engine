#pragma once

#include <memory>
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/transform.hpp>

#include <cstdint>
#include <fmt/core.h>
#include <vector>
#include <vulkan/vk_enum_string_helper.h>
#include <vk_mem_alloc.h>

namespace Engine {
const uint32_t WIDTH = 800;
const uint32_t HEIGHT = 600;

const std::string MODEL_PATH = "../../models/viking_room.obj";
const std::string TEXTURE_PATH = "../../textures/viking_room.png";

const std::vector<const char*> c_validationLayers = {
	"VK_LAYER_KHRONOS_validation"
};

const std::vector<const char*> c_deviceExtensions = {
	VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

enum class MaterialPass : uint8_t {
	MainColor,
	Transparent,
	Other
};

struct MaterialPipeline {
	VkPipeline pipeline;
	VkPipelineLayout layout;
};

struct MaterialInstance {
	MaterialPipeline* pipeline;
	VkDescriptorSet materialSet;
	MaterialPass passType;
};

struct ShadowCubeInstance {
	MaterialPipeline* pipeline;
	VkDescriptorSet materialSet;
};

struct ShadowInstance {
	MaterialPipeline* pipeline;
	VkDescriptorSet materialSet;
};

struct RenderObject {
	uint32_t indexCount;
	uint32_t firstIndex;
	VkBuffer indexBuffer;

	MaterialInstance* material;

	glm::mat4 transform;
	VkDeviceAddress vertexBufferAddress;
};

struct AllocatedImage {
	VkImage image;
	VkImageView imageView;
	VmaAllocation allocation;
	VkExtent3D imageExtent;
	VkFormat imageFormat;
};

struct AllocatedBuffer {
	VkBuffer buffer;
	VmaAllocation allocation;
	VmaAllocationInfo info;
};

struct PushConstants {
	glm::mat4 worldMatrix;
	glm::mat4 lightSpaceMatrix;
	VkDeviceAddress vertexBuffer;
};

struct DrawContext;

class IRenderable {
	virtual void draw(const glm::mat4& topMatrix, DrawContext& ctx) = 0;
};

struct Node : public IRenderable {
	std::weak_ptr<Node> parent;
	std::vector<std::shared_ptr<Node>> children;

	glm::mat4 localTransform;
	glm::mat4 worldTransform;

	std::shared_ptr<Node> clone() const {
		auto newNode = std::make_shared<Node>();

		newNode->localTransform = this->localTransform;
		newNode->worldTransform = this->worldTransform;
		newNode->parent = this->parent;

		for (const auto& child : children) {
			newNode->children.push_back(child->clone());
		}

		return newNode;
	}

	void refreshTransform(const glm::mat4& parentMatrix)
	{
		worldTransform = parentMatrix * localTransform;
		for (auto c : children) {
			c->refreshTransform(worldTransform);
		}
	}

	virtual void draw(const glm::mat4& topMatrix, DrawContext& ctx)
	{
		for (auto& c : children) {
			c->draw(topMatrix, ctx);
		}
	}
};

#ifdef NDEBUG
const bool c_enableValidationLayers = false;
#else
const bool c_enableValidationLayers = true;
#endif

#define VK_CHECK(x)                                                     \
do {                                                                \
	VkResult err = x;                                               \
if (err) {                                                      \
		fmt::println(fmt::runtime("Detected Vulkan error: {}"), string_VkResult(err)); \
		abort();                                                    \
	}                                                               \
} while (0)

}

