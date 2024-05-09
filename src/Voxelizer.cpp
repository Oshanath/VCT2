#include "Voxelizer.h"

Voxelizer::Voxelizer(std::shared_ptr<Helper> helper, uint32_t voxelsPerSide, glm::vec4 corner1, glm::vec4 corner2):
	helper(helper), voxelsPerSide(voxelsPerSide), aabbMin(glm::vec4(0.0f)), aabbMax(glm::vec4(0.0f)), center(glm::vec3(0.0f))
{
	helper->createImage(voxelsPerSide, voxelsPerSide, voxelsPerSide, 1, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_STORAGE_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, voxelTexture, voxelTextureMemory);
	voxelTextureView = helper->createImageView(voxelTexture, 1, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_VIEW_TYPE_3D);
	helper->setNameOfObject(VK_OBJECT_TYPE_IMAGE, (uint64_t)voxelTexture, "Voxel Texture");
	helper->setNameOfObject(VK_OBJECT_TYPE_IMAGE_VIEW, (uint64_t)voxelTextureView, "Voxel Texture View");



	// Transition image layout
	VkImageMemoryBarrier barrier = {};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
	barrier.image = voxelTexture;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = 1;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;
	barrier.srcAccessMask = 0;
	barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;

	VkCommandBuffer commandBuffer = helper->beginSingleTimeCommands();
	vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
	helper->endSingleTimeCommands(commandBuffer);

	calculateAABBMinMaxCenter(corner1, corner2);

	createUniformBuffers();
	createDescriptorSetLayouts();
	createDescriptorSets();
}

Voxelizer::~Voxelizer()
{
	vkDestroyDescriptorSetLayout(helper->device, voxelGridDescriptorSetLayout, nullptr);
	vkDestroyDescriptorSetLayout(helper->device, voxelTextureDescriptorSetLayout, nullptr);

	vkDestroyImageView(helper->device, voxelTextureView, nullptr);
	vkDestroyImage(helper->device, voxelTexture, nullptr);
	vkFreeMemory(helper->device, voxelTextureMemory, nullptr);

	for (size_t i = 0; i < helper->MAX_FRAMES_IN_FLIGHT; i++)
	{
		vkDestroyBuffer(helper->device, voxelGridUniformBuffers[i], nullptr);
		vkFreeMemory(helper->device, voxelGridUniformBuffersMemory[i], nullptr);

		vkDestroyBuffer(helper->device, transformsUniformBuffers[i], nullptr);
		vkFreeMemory(helper->device, transformsUniformBuffersMemory[i], nullptr);
	}

}

void Voxelizer::calculateAABBMinMaxCenter(glm::vec4 corner1, glm::vec4 corner2)
{
	glm::vec4 center4 = (corner1 + corner2) / 2.0f;
	float longestSide = std::max(corner2.x - corner1.x, std::max(corner2.y - corner1.y, corner2.z - corner1.z));
	center = glm::vec3(center4);

	aabbMin = glm::vec4(center - glm::vec3(longestSide / 2.0f), 0.0f);
	aabbMax = glm::vec4(center + glm::vec3(longestSide / 2.0f), 0.0f);
}

void Voxelizer::createUniformBuffers()
{
	VkDeviceSize bufferSize = sizeof(VoxelGridUBO);
	VkDeviceSize transformsBufferSize = sizeof(ViewProjectionMatrices);

	voxelGridUniformBuffers.resize(helper->MAX_FRAMES_IN_FLIGHT);
	voxelGridUniformBuffersMemory.resize(helper->MAX_FRAMES_IN_FLIGHT);
	voxelGridUniformBuffersMapped.resize(helper->MAX_FRAMES_IN_FLIGHT);

	transformsUniformBuffers.resize(helper->MAX_FRAMES_IN_FLIGHT);
	transformsUniformBuffersMemory.resize(helper->MAX_FRAMES_IN_FLIGHT);
	transformsUniformBuffersMapped.resize(helper->MAX_FRAMES_IN_FLIGHT);

	for (size_t i = 0; i < helper->MAX_FRAMES_IN_FLIGHT; i++)
	{
		helper->createBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, voxelGridUniformBuffers[i], voxelGridUniformBuffersMemory[i]);
		vkMapMemory(helper->device, voxelGridUniformBuffersMemory[i], 0, bufferSize, 0, &voxelGridUniformBuffersMapped[i]);

		helper->createBuffer(transformsBufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, transformsUniformBuffers[i], transformsUniformBuffersMemory[i]);
		vkMapMemory(helper->device, transformsUniformBuffersMemory[i], 0, transformsBufferSize, 0, &transformsUniformBuffersMapped[i]);
	}

}

ViewProjectionMatrices Voxelizer::getViewProjectionMatrices()
{
	ViewProjectionMatrices vp = {};

	glm::vec3 min = glm::vec3(aabbMin);
	glm::vec3 max = glm::vec3(aabbMax);
	float length = max.x - min.x;

	vp.view = glm::lookAt(center, center + glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, 1.0f, 0.0f));
	vp.proj = glm::ortho(-length / 2.0f, length / 2.0f, -length / 2.0f, length / 2.0f, -length / 2.0f, length / 2.0f);

	return vp;
}

void Voxelizer::updateUniformBuffers(uint32_t currentFrame)
{
	VoxelGridUBO ubo = {};
	ubo.aabbMin = aabbMin;
	ubo.aabbMax = aabbMax;
	memcpy(voxelGridUniformBuffersMapped[currentFrame], &ubo, sizeof(ubo));

	ViewProjectionMatrices vp = getViewProjectionMatrices();
	memcpy(transformsUniformBuffersMapped[currentFrame], &vp, sizeof(vp));
}

void Voxelizer::createDescriptorSetLayouts()
{
	VkDescriptorSetLayoutBinding transformsLayoutBinding = {};
	transformsLayoutBinding.binding = 0;
	transformsLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	transformsLayoutBinding.descriptorCount = 1;
	transformsLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
	transformsLayoutBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutBinding voxelGridLayoutBinding = {};
	voxelGridLayoutBinding.binding = 1;
	voxelGridLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	voxelGridLayoutBinding.descriptorCount = 1;
	voxelGridLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	voxelGridLayoutBinding.pImmutableSamplers = nullptr;

	std::vector<VkDescriptorSetLayoutBinding> bindings = { transformsLayoutBinding, voxelGridLayoutBinding };

	VkDescriptorSetLayoutCreateInfo layoutInfo = {};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
	layoutInfo.pBindings = bindings.data();

	if (vkCreateDescriptorSetLayout(helper->device, &layoutInfo, nullptr, &voxelGridDescriptorSetLayout) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create descriptor set layout!");
	}

	VkDescriptorSetLayoutBinding voxelTextureLayoutBinding = {};
	voxelTextureLayoutBinding.binding = 0;
	voxelTextureLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	voxelTextureLayoutBinding.descriptorCount = 1;
	voxelTextureLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	voxelTextureLayoutBinding.pImmutableSamplers = nullptr;
	
	std::vector<VkDescriptorSetLayoutBinding> bindings2 = { voxelTextureLayoutBinding };

	VkDescriptorSetLayoutCreateInfo layoutInfo2 = {};
	layoutInfo2.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo2.bindingCount = static_cast<uint32_t>(bindings2.size());
	layoutInfo2.pBindings = bindings2.data();

	if (vkCreateDescriptorSetLayout(helper->device, &layoutInfo2, nullptr, &voxelTextureDescriptorSetLayout) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create descriptor set layout!");
	}
}

void Voxelizer::createDescriptorSets()
{
	std::vector<VkDescriptorSetLayout> layouts(helper->MAX_FRAMES_IN_FLIGHT, voxelGridDescriptorSetLayout);
	VkDescriptorSetAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = helper->descriptorPool;
	allocInfo.descriptorSetCount = static_cast<uint32_t>(helper->MAX_FRAMES_IN_FLIGHT);
	allocInfo.pSetLayouts = layouts.data();

	voxelGridDescriptorSets.resize(helper->MAX_FRAMES_IN_FLIGHT);
	if (vkAllocateDescriptorSets(helper->device, &allocInfo, voxelGridDescriptorSets.data()) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to allocate descriptor sets!");
	}

	VkDescriptorSetAllocateInfo allocInfo2 = {};
	allocInfo2.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo2.descriptorPool = helper->descriptorPool;
	allocInfo2.descriptorSetCount = 1;
	allocInfo2.pSetLayouts = &voxelTextureDescriptorSetLayout;

	if (vkAllocateDescriptorSets(helper->device, &allocInfo2, &voxelTextureDescriptorSet) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to allocate descriptor sets!");
	}

	for (size_t i = 0; i < helper->MAX_FRAMES_IN_FLIGHT; i++)
	{
		VkDescriptorBufferInfo transformsBufferInfo = {};
		transformsBufferInfo.buffer = transformsUniformBuffers[i];
		transformsBufferInfo.offset = 0;
		transformsBufferInfo.range = sizeof(ViewProjectionMatrices);

		VkDescriptorBufferInfo voxelGridBufferInfo = {};
		voxelGridBufferInfo.buffer = voxelGridUniformBuffers[i];
		voxelGridBufferInfo.offset = 0;
		voxelGridBufferInfo.range = sizeof(VoxelGridUBO);

		std::vector<VkWriteDescriptorSet> descriptorWrites = {};

		VkWriteDescriptorSet transformsDescriptorWrite = {};
		transformsDescriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		transformsDescriptorWrite.dstSet = voxelGridDescriptorSets[i];
		transformsDescriptorWrite.dstBinding = 0;
		transformsDescriptorWrite.dstArrayElement = 0;
		transformsDescriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		transformsDescriptorWrite.descriptorCount = 1;
		transformsDescriptorWrite.pBufferInfo = &transformsBufferInfo;

		VkWriteDescriptorSet voxelGridDescriptorWrite = {};
		voxelGridDescriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		voxelGridDescriptorWrite.dstSet = voxelGridDescriptorSets[i];
		voxelGridDescriptorWrite.dstBinding = 1;
		voxelGridDescriptorWrite.dstArrayElement = 0;
		voxelGridDescriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		voxelGridDescriptorWrite.descriptorCount = 1;
		voxelGridDescriptorWrite.pBufferInfo = &voxelGridBufferInfo;

		descriptorWrites.push_back(transformsDescriptorWrite);
		descriptorWrites.push_back(voxelGridDescriptorWrite);

		vkUpdateDescriptorSets(helper->device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
	}

	VkDescriptorImageInfo imageInfo = {};
	imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	imageInfo.imageView = voxelTextureView;
	imageInfo.sampler = VK_NULL_HANDLE;

	VkWriteDescriptorSet descriptorWrite = {};
	descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descriptorWrite.dstSet = voxelTextureDescriptorSet;
	descriptorWrite.dstBinding = 0;
	descriptorWrite.dstArrayElement = 0;
	descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	descriptorWrite.descriptorCount = 1;
	descriptorWrite.pImageInfo = &imageInfo;

	vkUpdateDescriptorSets(helper->device, 1, &descriptorWrite, 0, nullptr);
}