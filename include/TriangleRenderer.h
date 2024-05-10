#ifndef TRIANGLE_RENDERER_H
#define TRIANGLE_RENDERER_H

#include "Application.h"
#include <array>
#include "Mesh.h"
#include "RenderObject.h"
#include "ShadowMap.h"
#include "GeometryVoxelizer.h"
#include "Camera.h"

struct MeshPushConstants {
	glm::mat4 model;
	float occlusionDecayFactor;
	VkBool32 ambientOcclusionEnabled;
	VkBool32 occlusionVisualizationEnabled;
	float surfaceOffset;
	float coneCutoff;
};

struct LightSpaceMatrix {
	glm::mat4 model;
};

struct TransformationUniformBufferObject {
	glm::mat4 view;
	glm::mat4 projection;
	glm::vec4 cameraPosition;
};

class TriangleRenderer : public Application
{
private:

	MeshPushConstants meshPushConstants;

	std::shared_ptr<Camera> camera;
	std::vector<std::shared_ptr<Model>> models;
	std::vector<std::shared_ptr<RenderObject>> renderObjects;

	VkDescriptorSetLayout descriptorSetLayout;
	VkPipelineLayout pipelineLayout;
	VkPipeline graphicsPipeline;

	std::vector<VkBuffer> transformationUniformBuffers;
	std::vector<VkDeviceMemory> transformationUniformBuffersMemory;
	std::vector<void*> transformationUniformBuffersMapped;

	std::shared_ptr<LightUBO> lightUBO;
	std::vector<VkBuffer> lightUniformBuffers;
	std::vector<VkDeviceMemory> lightUniformBuffersMemory;
	std::vector<void*> lightUniformBuffersMapped;

	std::vector<VkBuffer> lightSpaceMatrixUniformBuffers;
	std::vector<VkDeviceMemory> lightSpaceMatrixUniformBuffersMemory;
	std::vector<void*> lightSpaceMatrixUniformBuffersMapped;

	std::vector<VkDescriptorSet> descriptorSets;

	std::unique_ptr<ShadowMap> shadowMap;

	std::shared_ptr<Voxelizer> voxelizer;

	glm::vec4 corner1 = glm::vec4(-2153.88, 1446.43, 1338.9, 1.0f);
	glm::vec4 corner2 = glm::vec4(1879.78, -160.896, -1264.38f, 1.0f);
	bool enableVoxelVis = false;

public:
	TriangleRenderer(std::string app_name);

	void main_loop_extended(uint32_t currentFrame, uint32_t imageIndex) override;
	void cleanup_extended() override;
	void createGraphicsPipeline();
	void recordCommandBuffer(uint32_t currentFrame, uint32_t imageIndex) override;
	void beginRenderPass(uint32_t currentFrame, uint32_t imageIndex);
	void setDynamicState();
	void createBuffers();
	void createDescriptorSetLayouts();
	void updateUniformBuffers(uint32_t currentFrame);
	void createDescriptorSets();
	void key_callback_extended(GLFWwindow* window, int key, int scancode, int action, int mods, double deltaTime) override;
	void mouse_callback_extended(GLFWwindow* window, int button, int action, int mods, double deltaTime) override;
	void cursor_position_callback_extended(GLFWwindow* window, double xpos, double ypos) override;
	void renderScene();
	void revoxelize(int resolution);
};

#endif // !TRIANGLE_RENDERER_H