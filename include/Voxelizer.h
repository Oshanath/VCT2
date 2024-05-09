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

struct CubeModelViewProjectionMatrices
{
	glm::mat4 model;
	glm::mat4 view;
	glm::mat4 projection;
};

class Voxelizer
{
private:
	void calculateAABBMinMaxCenter(glm::vec4 corner1, glm::vec4 corner2);

public:
	const int INSTANCE_BUFFER_SIZE = 2000000;

	std::vector<Vertex> unitCubeVertices;
	std::vector<uint32_t> unitCubeIndices;
	VkBuffer unitCubeVertexBuffer;
	VkDeviceMemory unitCubeVertexBufferMemory;
	VkBuffer unitCubeIndexBuffer;
	VkDeviceMemory unitCubeIndexBufferMemory;

	const uint32_t voxelsPerSide;
	float length;
	float voxelWidth;
	glm::vec4 aabbMin;
	glm::vec4 aabbMax;
	glm::vec3 center;

	std::shared_ptr<Helper> helper;

	VkImage voxelTexture;
	VkDeviceMemory voxelTextureMemory;
	VkImageView voxelTextureView;
	VkDescriptorSetLayout voxelTextureDescriptorSetLayout;
	VkDescriptorSet voxelTextureDescriptorSet;

	std::vector<VkImageView> voxelTextureMipViews;
	uint32_t mipLevelCount;

	std::vector<VkBuffer> transformsUniformBuffers;
	std::vector<VkDeviceMemory> transformsUniformBuffersMemory;
	std::vector<void*> transformsUniformBuffersMapped;

	std::vector<VkBuffer> voxelGridUniformBuffers;
	std::vector<VkDeviceMemory> voxelGridUniformBuffersMemory;
	std::vector<void*> voxelGridUniformBuffersMapped;

	std::vector<VkBuffer> cubeTransformsUniformBuffers;
	std::vector<VkDeviceMemory> cubeTransformsUniformBuffersMemory;
	std::vector<void*> cubeTransformsUniformBuffersMapped;

	VkPipeline voxelVisComputePipeline;
	VkPipelineLayout voxelVisComputePipelineLayout;
	VkPipeline voxelVisResetIndirectBufferComputePipeline;
	VkPipelineLayout voxelVisResetIndirectBufferComputePipelineLayout;
	VkPipeline voxelVisGraphicsPipeline;
	VkPipelineLayout voxelVisGraphicsPipelineLayout;
	VkPipeline mipMapperComputePipeline;
	VkPipelineLayout mipMapperComputePipelineLayout;

	VkDescriptorSetLayout voxelGridDescriptorSetLayout;
	std::vector<VkDescriptorSet> voxelGridDescriptorSets;
	VkDescriptorSetLayout voxelVisInstanceBufferDescriptorSetLayout;
	VkDescriptorSet voxelVisInstanceBufferDescriptorSet;
	VkDescriptorSetLayout voxelVisIndirectBufferDescriptorSetLayout;
	VkDescriptorSet voxelVisIndirectBufferDescriptorSet;
	VkDescriptorSetLayout voxelVisCubeTransformsUBODescriptorSetLayout;
	VkDescriptorSet voxelVisCubeTransformsUBODescriptorSet;
	VkDescriptorSetLayout mipMapperDescriptorSetLayout;
	VkDescriptorSet mipMapperDescriptorSet;

	VkBuffer instancePositionsBuffer;
	VkDeviceMemory instancePositionsBufferMemory;
	VkBuffer instanceColorsBuffer;
	VkDeviceMemory instanceColorsBufferMemory;
	VkBuffer indirectDrawBuffer;
	VkDeviceMemory indirectDrawBufferMemory;
	VkBuffer mipMapperAtomicCountersBuffer;
	VkDeviceMemory mipMapperAtomicCountersBufferMemory;

	virtual void beginVoxelization(VkCommandBuffer commandBuffer, uint32_t currentFrame) = 0;
	virtual void voxelize(VkCommandBuffer commandBuffer, uint32_t currentFrame) = 0;
	virtual void endVoxelization(VkCommandBuffer commandBuffer, uint32_t currentFrame) = 0;

	Voxelizer(std::shared_ptr<Helper> helper, uint32_t voxelsPerSide, glm::vec4 corner1, glm::vec4 corner2);
	~Voxelizer();

	void createBuffers();
	void updateUniformBuffers(uint32_t currentFrame);
	void createDescriptorSetLayouts();
	void createDescriptorSets();
	ViewProjectionMatrices getViewProjectionMatrices();
	void createVoxelVisResources();
	void createVoxelVisComputePipeline();
	void dispatchVoxelVisComputeShader(VkCommandBuffer commandBuffer, uint32_t currentFrame);
	void createVoxelVisResetIndirectBufferComputePipeline();
	void dispatchVoxelVisResetIndirectBufferComputeShader(VkCommandBuffer commandBuffer, uint32_t currentFrame);
	void createVoxelVisGraphicsPipeline();
	void visualizeVoxelGrid(VkCommandBuffer commandBuffer, uint32_t currentFrame);
	void createCubeVertexindexBuffers();
	void createMipMapperComputePipeline();
	void generateMipMaps(VkCommandBuffer commandBuffer, uint32_t currentFrame);
};

#endif // !VOXELIZER_H
