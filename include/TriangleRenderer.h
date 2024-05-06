#ifndef TRIANGLE_RENDERER_H
#define TRIANGLE_RENDERER_H

#include "Application.h"
#include <array>
#include "Mesh.h"
#include "RenderObject.h"

struct MeshPushConstants {
	glm::mat4 model;
};

class TriangleRenderer : public Application
{
private:

	Camera camera;
	std::vector<std::shared_ptr<Model>> models;
	std::vector<std::shared_ptr<RenderObject>> renderObjects;

	VkShaderModule vertShaderModule;
	VkShaderModule fragShaderModule;

	VkDescriptorSetLayout descriptorSetLayout;
	VkPipelineLayout pipelineLayout;
	VkPipeline graphicsPipeline;

	std::vector<VkBuffer> uniformBuffers;
	std::vector<VkDeviceMemory> uniformBuffersMemory;
	std::vector<void*> uniformBuffersMapped;

	std::vector<VkDescriptorSet> descriptorSets;

public:
	TriangleRenderer(std::string app_name);

	void main_loop_extended(uint32_t currentFrame, uint32_t imageIndex) override;
	void cleanup_extended() override;
	void createGraphicsPipeline();
	VkShaderModule createShaderModule(const std::vector<char>& code);
	void recordCommandBuffer(uint32_t currentFrame, uint32_t imageIndex) override;
	void beginRenderPass(uint32_t currentFrame, uint32_t imageIndex);
	void setDynamicState();
	void createUniformBuffers();
	void createDescriptorSetLayout();
	void updateUniformBuffer(uint32_t currentFrame);
	void createDescriptorSets();
	void key_callback_extended(GLFWwindow* window, int key, int scancode, int action, int mods, double deltaTime) override;
	void mouse_callback_extended(GLFWwindow* window, int button, int action, int mods, double deltaTime) override;
	void cursor_position_callback_extended(GLFWwindow* window, double xpos, double ypos) override;
};

#endif // !TRIANGLE_RENDERER_H