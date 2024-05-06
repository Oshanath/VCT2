#ifndef SHADOW_MAP_H
#define SHADOW_MAP_H

#include "Helper.h"

#include <memory>

class ShadowMap
{
public:
	std::shared_ptr<Helper> helper;

	VkImage image;
	VkDeviceMemory imageMemory;
	VkImageView imageView;
	VkSampler sampler;
	VkRenderPass renderPass;
	VkFramebuffer framebuffer;


};

#endif // !SHADOW_MAP_H
