#ifndef VOXELIZER_H
#define VOXELIZER_H

#include "Helper.h"
#include "Camera.h"
#include "Mesh.h"
#include <glm/glm.hpp>

enum VoxelizationType
{
	GEOMETRY_SHADER_VOXELIZATION,
	COMPUTE_SHADER_VOXELIZATION,
	MESH_SHADER_VOXELIZATION
};

struct VoxelGridUBO
{
	glm::vec4 aabbMin;
	glm::vec4 aabbMax;
};

class Voxelizer
{
private:
	void calculateAABBMinMaxCenter(glm::vec4 corner1, glm::vec4 corner2);

public:
	const uint32_t voxelsPerSide;
	glm::vec4 aabbMin;
	glm::vec4 aabbMax;
	glm::vec3 center;

	std::shared_ptr<Helper> helper;

	VkImage voxelTexture;
	VkDeviceMemory voxelTextureMemory;
	VkImageView voxelTextureView;
	VkDescriptorSetLayout voxelTextureDescriptorSetLayout;
	VkDescriptorSet voxelTextureDescriptorSet;

	std::vector<VkBuffer> transformsUniformBuffers;
	std::vector<VkDeviceMemory> transformsUniformBuffersMemory;
	std::vector<void*> transformsUniformBuffersMapped;

	std::vector<VkBuffer> voxelGridUniformBuffers;
	std::vector<VkDeviceMemory> voxelGridUniformBuffersMemory;
	std::vector<void*> voxelGridUniformBuffersMapped;

	VkPipeline voxelVisComputePipeline;
	VkPipelineLayout voxelVisComputePipelineLayout;
	VkPipeline voxelVisResetIndirectBufferComputePipeline;
	VkPipelineLayout voxelVisResetIndirectBufferComputePipelineLayout;
	VkShaderModule voxelVisComputeShaderModule;
	VkDescriptorSetLayout voxelGridDescriptorSetLayout;
	std::vector<VkDescriptorSet> voxelGridDescriptorSets;
	VkDescriptorSetLayout voxelVisInstanceBufferDescriptorSetLayout;
	VkDescriptorSet voxelVisInstanceBufferDescriptorSet;
	VkDescriptorSetLayout voxelVisIndirectBufferDescriptorSetLayout;
	VkDescriptorSet voxelVisIndirectBufferDescriptorSet;

	VkBuffer instancePositionsBuffer;
	VkDeviceMemory instancePositionsBufferMemory;
	VkBuffer instanceColorsBuffer;
	VkDeviceMemory instanceColorsBufferMemory;
	VkBuffer indirectDrawBuffer;
	VkDeviceMemory indirectDrawBufferMemory;

	virtual void beginVoxelization(VkCommandBuffer commandBuffer, uint32_t currentFrame) = 0;
	virtual void voxelize(VkCommandBuffer commandBuffer, uint32_t currentFrame) = 0;
	virtual void endVoxelization(VkCommandBuffer commandBuffer, uint32_t currentFrame) = 0;

	Voxelizer(std::shared_ptr<Helper> helper, uint32_t voxelsPerSide, glm::vec4 corner1, glm::vec4 corner2);
	~Voxelizer();

	void createUniformBuffers();
	void updateUniformBuffers(uint32_t currentFrame);
	void createDescriptorSetLayouts();
	void createDescriptorSets();
	ViewProjectionMatrices getViewProjectionMatrices();
	void createVoxelVisResources();
	void createVoxelVisComputePipeline();
	void dispatchVoxelVisComputeShader(VkCommandBuffer commandBuffer, uint32_t currentFrame);
	void createVoxelVisResetIndirectBufferComputePipeline();
	void dispatchVoxelVisResetIndirectBufferComputeShader(VkCommandBuffer commandBuffer, uint32_t currentFrame);
};

#endif // !VOXELIZER_H
