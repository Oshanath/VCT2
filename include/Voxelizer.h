#ifndef VOXELIZER_H
#define VOXELIZER_H

#include "Helper.h"

enum VoxelizationType
{
	GEOMETRY_SHADER_VOXELIZATION,
	COMPUTE_SHADER_VOXELIZATION,
	MESH_SHADER_VOXELIZATION
};

class Voxelizer
{
public:
	const uint32_t voxelsPerSize;

	std::shared_ptr<Helper> helper;

	VkImage voxelTexture;
	VkDeviceMemory voxelTextureMemory;
	VkImageView voxelTextureView;

	Voxelizer(std::shared_ptr<Helper> helper, uint32_t voxelsPerSize);
};

#endif // !VOXELIZER_H
