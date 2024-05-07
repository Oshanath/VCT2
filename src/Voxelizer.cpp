#include "Voxelizer.h"

Voxelizer::Voxelizer(std::shared_ptr<Helper> helper, uint32_t voxelsPerSize):
	helper(helper), voxelsPerSize(voxelsPerSize)
{
	helper->createImage(voxelsPerSize, voxelsPerSize, voxelsPerSize, 1, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_USAGE_STORAGE_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, voxelTexture, voxelTextureMemory);
	voxelTextureView = helper->createImageView(voxelTexture, 1, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_VIEW_TYPE_3D);
}