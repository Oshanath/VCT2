#ifndef GEOMETRY_VOXELIZER_H
#define GEOMETRY_VOXELIZER_H

#include "Voxelizer.h"

class GeometryVoxelizer : public Voxelizer
{
public:
	VkPipelineLayout voxelGridPipelineLayout;
	VkPipeline voxelGridGraphicsPipeline;
	VkShaderModule vertexShaderModule;
	VkShaderModule geometryShaderModule;
	VkShaderModule fragmentShaderModule;
	VkRenderPass voxelizationRenderPass;
	VkFramebuffer voxelizationFrameBuffer;

	GeometryVoxelizer(std::shared_ptr<Helper> helper, uint32_t voxelsPerSide, glm::vec4 corner1, glm::vec4 corner2);
	~GeometryVoxelizer();

	void beginVoxelization(VkCommandBuffer commandBuffer, uint32_t currentFrame) override;
	void voxelize(VkCommandBuffer commandBuffer, uint32_t currentFrame) override;
	void endVoxelization(VkCommandBuffer commandBuffer, uint32_t currentFrame) override;

	void createVoxelizationGraphicsPipeline();
	void createVoxelizationRenderPassFrameBuffer();

};

#endif // !GEOMETRY_VOXELIZER_H
