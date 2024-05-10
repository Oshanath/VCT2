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
	uint32_t width = 20000;

	float leftPlane = -3000.0f;
	float rightPlane = 3000.0f;
	float bottomPlane = -3000.0f;
	float topPlane = 3000.0f;
	float nearPlane = -1000.0f;
	float farPlane = 2000.0f;
	float backOffDistance = 1000.0f;
	glm::vec3 target = glm::vec3(0.0f, 0.0f, 0.0f);
	float depthBiasConstant = 1.25f;
	float depthBiasSlope = 1.75f;

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
	VkDescriptorSetLayout shadowMapDescriptorSetLayout;
	VkDescriptorSet shadowMapDescriptorSet;
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
	glm::mat4 getLightSpaceMatrix();
};

#endif // !SHADOW_MAP_H
