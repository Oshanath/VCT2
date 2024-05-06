#ifndef HELPER_H
#define HELPER_H

#include <vulkan/vulkan.h>
#include <string>
#include <stb_image.h>
#include <fstream>
#include <stdexcept>
#include <cmath>
#include <iostream>
#include <vector>

class Helper
{
public:
	VkCommandPool commandPool;
	VkDevice device;
	VkQueue graphicsQueue;
	VkPhysicalDevice physicalDevice;
	VkDescriptorPool descriptorPool;
	VkInstance instance;

	Helper(VkCommandPool commandPool, VkDevice device, VkQueue graphicsQueue, VkPhysicalDevice physicalDevice, VkDescriptorPool descriptorPool, VkInstance instance);
	Helper();

	VkCommandBuffer beginSingleTimeCommands();
	void endSingleTimeCommands(VkCommandBuffer commandBuffer);
	void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory);
	uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
	void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
	void createTextureImage(std::string path, VkImage& textureImage, VkDeviceMemory& textureImageMemory, VkImageView& textureImageView, uint32_t* mipLevels = nullptr);
	void createImage(uint32_t width, uint32_t height, uint32_t depth, uint32_t mipLevels, VkFormat format, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory);
	VkImageView createImageView(VkImage image, uint32_t mipLevels, VkFormat format, VkImageAspectFlagBits aspectFlags);
	void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);
	void createSampler(VkSampler& textureSampler, uint32_t mipLevels = 0);
	void generateMipmaps(VkImage image, int32_t texWidth, int32_t texHeight, uint32_t mipLevels);
	std::vector<char> readFile(const std::string& filename);
	VkShaderModule createShaderModule(const std::vector<char>& code);
	void setNameOfObject(VkObjectType type, uint64_t objectHandle, std::string name);
};


#endif // !HELPER_H
