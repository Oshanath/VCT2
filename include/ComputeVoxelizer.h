#ifndef COMPUTE_VOXELIZER_H
#define COMPUTE_VOXELIZER_H

#include "Voxelizer.h"

struct LargeTriangle
{
	uint32_t triangleIndex;
	uint32_t innerTriangleIndex;
};

struct computeVoxelizationPushConstants
{
	glm::mat4 model;
	int32_t triangleCount;
};

class ComputeVoxelizer : public Voxelizer
{
public:
	const int LARGE_TRIANGLE_BUFFER_SIZE = 10000;

	// Indirect dispatch buffer
	VkBuffer indirectDispatchBuffer;
	VkDeviceMemory indirectDispatchBufferMemory;
	VkDescriptorSetLayout indirectDispatchBufferDescriptorSetLayout;
	VkDescriptorSet indirectDispatchBufferDescriptorSet;

	// Large triangle buffer
	VkBuffer largeTriangleBuffer;
	VkDeviceMemory largeTriangleBufferMemory;
	VkDescriptorSetLayout largeTriangleBufferDescriptorSetLayout;
	VkDescriptorSet largeTriangleBufferDescriptorSet;

	// Small triangles pipeline
	VkPipeline smallTrianglesComputePipeline;
	VkPipelineLayout smallTrianglesComputePipelineLayout;

	ComputeVoxelizer(std::shared_ptr<Helper> helper, uint32_t voxelsPerSide, glm::vec4 corner1, glm::vec4 corner2);
	~ComputeVoxelizer();

	void createBuffers();
	void createDescriptorSetLayouts();
	void createDescriptorSets();
	void createSmallTrianglesComputePipeline();
	void beginVoxelization(VkCommandBuffer commandBuffer, uint32_t currentFrame) override;
};

#endif // !COMPUTE_VOXELIZER_H
