#ifndef SHADOW_MAP_H
#define SHADOW_MAP_H

#include "Helper.h"
#include "Camera.h"

#include <memory>
#include <glm/glm.hpp>

#include "Mesh.h"

struct LightUBO {
	glm::vec4 direction;
};

class ShadowMap
{
public:
	uint32_t width = 2000;

	std::shared_ptr<Helper> helper;
	std::shared_ptr<LightUBO> light;

	VkImage image;
	VkDeviceMemory imageMemory;
	VkImageView imageView;
	VkSampler sampler;
	VkRenderPass renderPass;
	VkFramebuffer framebuffer;
	VkDescriptorSetLayout descriptorSetLayout;
	VkDescriptorSet descriptorSet;
	VkPipeline graphicsPipeline;
	VkPipelineLayout pipelineLayout;
	VkShaderModule vertShaderModule;
	VkShaderModule fragShaderModule;

	VkBuffer uniformBuffer;
	VkDeviceMemory uniformBufferMemory;
	void* uniformBufferMapped;

	ShadowMap(std::shared_ptr<Helper> helper, std::shared_ptr<LightUBO> light);
	~ShadowMap();

	void createDescriptorSets();
	void createPipeline();
	void beginRender(VkCommandBuffer commandBuffer);
	void endRender(VkCommandBuffer commandBuffer);
};

#endif // !SHADOW_MAP_H
