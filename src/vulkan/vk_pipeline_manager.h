#pragma once

#include <optional>
#include <string>
#include <unordered_map>
#include <vulkan/vulkan_core.h>
namespace Engine {

struct MaterialPipeline {
	VkPipeline pipeline;
	VkPipelineLayout layout;
};

struct PipelineContext {
	VkPipelineLayout layout;
	VkRenderPass renderPass;

	float vpWidth;
	float vpHeight;
	VkExtent2D scissorExtent;

	VkCullModeFlags cullMode;
	VkFrontFace frontFace;

	VkSampleCountFlagBits samples;

	VkCompareOp depthCompare;
	VkBool32 depthWriteEnable{ VK_TRUE };
	VkBool32 depthTestEnable{ VK_TRUE };

	std::optional<VkPipelineColorBlendAttachmentState*> colorBlendAttachment;
	std::optional<VkPipelineVertexInputStateCreateInfo> vertexInputState;

	VkPipelineShaderStageCreateInfo* shaderStages;
	uint32_t numShaders;
};

class PipelineManager {
public:
	void create(VkDevice device);
	void destroy();
	void create_pipeline(std::string& name, PipelineContext& context);
	void destroy_pipeline(std::string& name);
	MaterialPipeline get_pipeline(std::string& name);
private:
	std::unordered_map<std::string, MaterialPipeline> m_pipelineCache;
	VkDevice m_device;
};

}
