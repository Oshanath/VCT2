#ifndef MESH_H
#define MESH_H

#include "Helper.h"

#include <iostream>
#include <assimp/Importer.hpp>      // C++ importer interface
#include <assimp/scene.h>           // Output data structure
#include <assimp/postprocess.h>     // Post processing flags

#include <vulkan/vulkan.h>
#include <vector>
#include <glm/glm.hpp>
#include <array>
#include <memory>

struct Vertex {
	glm::vec3 pos;
	glm::vec3 normal;
	glm::vec2 texCoord;

	static VkVertexInputBindingDescription getBindingDescription()
	{
		VkVertexInputBindingDescription bindingDescription{};
		bindingDescription.binding = 0;
		bindingDescription.stride = sizeof(Vertex);
		bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

		return bindingDescription;
	}

	static std::array<VkVertexInputAttributeDescription, 3> getAttributeDescriptions()
	{
		std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions{};

		attributeDescriptions[0].binding = 0;
		attributeDescriptions[0].location = 0;
		attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
		attributeDescriptions[0].offset = offsetof(Vertex, pos);

		attributeDescriptions[1].binding = 0;
		attributeDescriptions[1].location = 1;
		attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
		attributeDescriptions[1].offset = offsetof(Vertex, normal);

		attributeDescriptions[2].binding = 0;
		attributeDescriptions[2].location = 2;
		attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
		attributeDescriptions[2].offset = offsetof(Vertex, texCoord);

		return attributeDescriptions;
	}
};

class Mesh
{
public:
	std::shared_ptr<Helper> helper;

	std::vector<Vertex> vertices;
	std::vector<uint32_t> indices;
	uint32_t materialIndex;

	VkBuffer vertexBuffer;
	VkDeviceMemory vertexBufferMemory;
	VkBuffer indexBuffer;
	VkDeviceMemory indexBufferMemory;

	Mesh(std::shared_ptr<Helper> helper, std::vector<Vertex>&& vertices, std::vector<uint32_t>&& indices, uint32_t materialIndex);
	~Mesh();

private:
	void createVertexBuffer();
	void createIndexBuffer();
};

class Model
{
public:
	inline static VkDescriptorSetLayout descriptorSetLayout;
	inline static bool descriptorSetLayoutCreated = false;

	std::shared_ptr<Helper> helper;
	std::string path;
	std::string directory;

	std::vector<std::unique_ptr<Mesh>> meshes;
	std::vector<VkImage> textureImages;
	std::vector<VkDeviceMemory> textureImagesMemory;
	std::vector<VkImageView> textureImageViews;
	VkSampler textureSampler;
	std::vector<VkDescriptorSet> descriptorSets;
	std::vector<uint32_t> mipLevels;

	Model(std::string path, std::shared_ptr<Helper> helper);
	~Model();

	inline static void createDescriptorSetLayouts(Helper& helper)
	{
		if (descriptorSetLayoutCreated)
			return;

		descriptorSetLayoutCreated = true;

		VkDescriptorSetLayoutBinding dsLayoutBinding = {};
		dsLayoutBinding.binding = 0;
		dsLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		dsLayoutBinding.descriptorCount = 1;
		dsLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		dsLayoutBinding.pImmutableSamplers = nullptr;

		VkDescriptorSetLayoutCreateInfo layoutInfo = {};
		layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		layoutInfo.bindingCount = 1;
		layoutInfo.pBindings = &dsLayoutBinding;

		if (vkCreateDescriptorSetLayout(helper.device, &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS) {
			throw std::runtime_error("Failed to create descriptor set layout");
		}
	}

	inline static VkDescriptorSetLayout getDescriptorSetLayout()
	{
		return descriptorSetLayout;
	}

	inline static void destroyDescriptorSetLayout(Helper& helper)
	{
		if (descriptorSetLayoutCreated)
		{
			vkDestroyDescriptorSetLayout(helper.device, descriptorSetLayout, nullptr);
			descriptorSetLayoutCreated = false;
		}
	}
};

#endif // !MESH_H
