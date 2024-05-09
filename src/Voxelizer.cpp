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
	createVoxelVisResources();
	createDescriptorSetLayouts();
	createDescriptorSets();
	createVoxelVisComputePipeline();
	createVoxelVisResetIndirectBufferComputePipeline();
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

	vkDestroyDescriptorSetLayout(helper->device, voxelVisInstanceBufferDescriptorSetLayout, nullptr);
	vkDestroyDescriptorSetLayout(helper->device, voxelVisIndirectBufferDescriptorSetLayout, nullptr);
	vkDestroyPipelineLayout(helper->device, voxelVisComputePipelineLayout, nullptr);
	vkDestroyPipeline(helper->device, voxelVisComputePipeline, nullptr);
	vkDestroyShaderModule(helper->device, voxelVisComputeShaderModule, nullptr);

	vkDestroyBuffer(helper->device, instancePositionsBuffer, nullptr);
	vkFreeMemory(helper->device, instancePositionsBufferMemory, nullptr);
	vkDestroyBuffer(helper->device, instanceColorsBuffer, nullptr);
	vkFreeMemory(helper->device, instanceColorsBufferMemory, nullptr);
	vkDestroyBuffer(helper->device, indirectDrawBuffer, nullptr);
	vkFreeMemory(helper->device, indirectDrawBufferMemory, nullptr);

	vkDestroyPipelineLayout(helper->device, voxelVisResetIndirectBufferComputePipelineLayout, nullptr);
	vkDestroyPipeline(helper->device, voxelVisResetIndirectBufferComputePipeline, nullptr);
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
	voxelGridLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT;
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
	voxelTextureLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT;
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

void Voxelizer::createVoxelVisResources()
{
	VkDeviceSize instancePositionsBufferSize = sizeof(glm::vec4) * 10000;
	VkDeviceSize instanceColorsBufferSize = sizeof(glm::vec4) * 10000;
	VkDeviceSize indirectDrawBufferSize = sizeof(VkDrawIndexedIndirectCommand);

	helper->createBuffer(instancePositionsBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, instancePositionsBuffer, instancePositionsBufferMemory);
	helper->createBuffer(instanceColorsBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, instanceColorsBuffer, instanceColorsBufferMemory);
	helper->createBuffer(indirectDrawBufferSize, VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, indirectDrawBuffer, indirectDrawBufferMemory);

	// Desciptor set layouts
	VkDescriptorSetLayoutBinding instancePositionsLayoutBinding = {};
	instancePositionsLayoutBinding.binding = 0;
	instancePositionsLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	instancePositionsLayoutBinding.descriptorCount = 1;
	instancePositionsLayoutBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_VERTEX_BIT;
	instancePositionsLayoutBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutBinding instanceColorsLayoutBinding = {};
	instanceColorsLayoutBinding.binding = 1;
	instanceColorsLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	instanceColorsLayoutBinding.descriptorCount = 1;
	instanceColorsLayoutBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
	instanceColorsLayoutBinding.pImmutableSamplers = nullptr;

	std::vector<VkDescriptorSetLayoutBinding> bindings = { instancePositionsLayoutBinding, instanceColorsLayoutBinding };

	VkDescriptorSetLayoutCreateInfo layoutInfo = {};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
	layoutInfo.pBindings = bindings.data();

	if (vkCreateDescriptorSetLayout(helper->device, &layoutInfo, nullptr, &voxelVisInstanceBufferDescriptorSetLayout) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create descriptor set layout!");
	}

	// Indirect draw buffer descriptor set layout
	VkDescriptorSetLayoutBinding indirectDrawLayoutBinding = {};
	indirectDrawLayoutBinding.binding = 0;
	indirectDrawLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	indirectDrawLayoutBinding.descriptorCount = 1;
	indirectDrawLayoutBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	indirectDrawLayoutBinding.pImmutableSamplers = nullptr;

	std::vector<VkDescriptorSetLayoutBinding> bindings2 = { indirectDrawLayoutBinding };

	VkDescriptorSetLayoutCreateInfo layoutInfo2 = {};
	layoutInfo2.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo2.bindingCount = static_cast<uint32_t>(bindings2.size());
	layoutInfo2.pBindings = bindings2.data();

	if (vkCreateDescriptorSetLayout(helper->device, &layoutInfo2, nullptr, &voxelVisIndirectBufferDescriptorSetLayout) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create descriptor set layout!");
	}

	// Descriptor sets
	VkDescriptorSetAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = helper->descriptorPool;
	allocInfo.descriptorSetCount = 1;
	allocInfo.pSetLayouts = &voxelVisInstanceBufferDescriptorSetLayout;

	if (vkAllocateDescriptorSets(helper->device, &allocInfo, &voxelVisInstanceBufferDescriptorSet) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to allocate descriptor sets!");
	}

	VkDescriptorBufferInfo instancePositionsBufferInfo = {};
	instancePositionsBufferInfo.buffer = instancePositionsBuffer;
	instancePositionsBufferInfo.offset = 0;
	instancePositionsBufferInfo.range = instancePositionsBufferSize;

	VkDescriptorBufferInfo instanceColorsBufferInfo = {};
	instanceColorsBufferInfo.buffer = instanceColorsBuffer;
	instanceColorsBufferInfo.offset = 0;
	instanceColorsBufferInfo.range = instanceColorsBufferSize;

	std::vector<VkWriteDescriptorSet> descriptorWrites = {};

	VkWriteDescriptorSet instancePositionsDescriptorWrite = {};
	instancePositionsDescriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	instancePositionsDescriptorWrite.dstSet = voxelVisInstanceBufferDescriptorSet;
	instancePositionsDescriptorWrite.dstBinding = 0;
	instancePositionsDescriptorWrite.dstArrayElement = 0;
	instancePositionsDescriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	instancePositionsDescriptorWrite.descriptorCount = 1;
	instancePositionsDescriptorWrite.pBufferInfo = &instancePositionsBufferInfo;

	VkWriteDescriptorSet instanceColorsDescriptorWrite = {};
	instanceColorsDescriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	instanceColorsDescriptorWrite.dstSet = voxelVisInstanceBufferDescriptorSet;
	instanceColorsDescriptorWrite.dstBinding = 1;
	instanceColorsDescriptorWrite.dstArrayElement = 0;
	instanceColorsDescriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	instanceColorsDescriptorWrite.descriptorCount = 1;
	instanceColorsDescriptorWrite.pBufferInfo = &instanceColorsBufferInfo;

	descriptorWrites.push_back(instancePositionsDescriptorWrite);
	descriptorWrites.push_back(instanceColorsDescriptorWrite);

	vkUpdateDescriptorSets(helper->device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);

	// Indirect draw buffer
	VkDescriptorSetAllocateInfo allocInfo2 = {};
	allocInfo2.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo2.descriptorPool = helper->descriptorPool;
	allocInfo2.descriptorSetCount = 1;
	allocInfo2.pSetLayouts = &voxelVisIndirectBufferDescriptorSetLayout;

	if (vkAllocateDescriptorSets(helper->device, &allocInfo2, &voxelVisIndirectBufferDescriptorSet) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to allocate descriptor sets!");
	}

	VkDescriptorBufferInfo indirectDrawBufferInfo = {};
	indirectDrawBufferInfo.buffer = indirectDrawBuffer;
	indirectDrawBufferInfo.offset = 0;
	indirectDrawBufferInfo.range = indirectDrawBufferSize;

	VkWriteDescriptorSet indirectDrawDescriptorWrite = {};
	indirectDrawDescriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	indirectDrawDescriptorWrite.dstSet = voxelVisIndirectBufferDescriptorSet;
	indirectDrawDescriptorWrite.dstBinding = 0;
	indirectDrawDescriptorWrite.dstArrayElement = 0;
	indirectDrawDescriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	indirectDrawDescriptorWrite.descriptorCount = 1;
	indirectDrawDescriptorWrite.pBufferInfo = &indirectDrawBufferInfo;

	vkUpdateDescriptorSets(helper->device, 1, &indirectDrawDescriptorWrite, 0, nullptr);
}

void Voxelizer::createVoxelVisComputePipeline()
{
	// Load compute shader pipeline
	auto computeShaderCode = helper->readFile("shaders/voxelVisPerVoxel.comp.spv");
	voxelVisComputeShaderModule = helper->createShaderModule(computeShaderCode);

	VkPipelineShaderStageCreateInfo computeShaderStageInfo = {};
	computeShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	computeShaderStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	computeShaderStageInfo.module = voxelVisComputeShaderModule;
	computeShaderStageInfo.pName = "main";

	std::vector<VkDescriptorSetLayout> layouts = {voxelTextureDescriptorSetLayout, voxelVisInstanceBufferDescriptorSetLayout, voxelVisIndirectBufferDescriptorSetLayout, voxelGridDescriptorSetLayout};
		
	VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = layouts.size();
	pipelineLayoutInfo.pSetLayouts = layouts.data();
	pipelineLayoutInfo.pushConstantRangeCount = 0;
	pipelineLayoutInfo.pPushConstantRanges = nullptr;

	if (vkCreatePipelineLayout(helper->device, &pipelineLayoutInfo, nullptr, &voxelVisComputePipelineLayout) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create pipeline layout!");
	}

	VkComputePipelineCreateInfo pipelineInfo = {};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	pipelineInfo.stage = computeShaderStageInfo;
	pipelineInfo.layout = voxelVisComputePipelineLayout;

	if (vkCreateComputePipelines(helper->device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &voxelVisComputePipeline) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create graphics pipeline!");
	}
}

void Voxelizer::createVoxelVisResetIndirectBufferComputePipeline()
{
	auto computeShaderCode = helper->readFile("shaders/voxelVisResetIndirectBuffer.comp.spv");
	VkShaderModule computeShaderModule = helper->createShaderModule(computeShaderCode);

	VkPipelineShaderStageCreateInfo computeShaderStageInfo = {};
	computeShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	computeShaderStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	computeShaderStageInfo.module = computeShaderModule;
	computeShaderStageInfo.pName = "main";

	VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = 1;
	pipelineLayoutInfo.pSetLayouts = &voxelVisIndirectBufferDescriptorSetLayout;
	pipelineLayoutInfo.pushConstantRangeCount = 0;
	pipelineLayoutInfo.pPushConstantRanges = nullptr;

	if (vkCreatePipelineLayout(helper->device, &pipelineLayoutInfo, nullptr, &voxelVisResetIndirectBufferComputePipelineLayout) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create pipeline layout!");
	}

	VkComputePipelineCreateInfo pipelineInfo = {};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	pipelineInfo.stage = computeShaderStageInfo;
	pipelineInfo.layout = voxelVisResetIndirectBufferComputePipelineLayout;

	if (vkCreateComputePipelines(helper->device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &voxelVisResetIndirectBufferComputePipeline) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create graphics pipeline!");
	}

	vkDestroyShaderModule(helper->device, computeShaderModule, nullptr);
}

void Voxelizer::dispatchVoxelVisComputeShader(VkCommandBuffer commandBuffer, uint32_t currentFrame)
{
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, voxelVisComputePipeline);
	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, voxelVisComputePipelineLayout, 0, 1, &voxelTextureDescriptorSet, 0, nullptr);
	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, voxelVisComputePipelineLayout, 1, 1, &voxelVisInstanceBufferDescriptorSet, 0, nullptr);
	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, voxelVisComputePipelineLayout, 2, 1, &voxelVisIndirectBufferDescriptorSet, 0, nullptr);
	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, voxelVisComputePipelineLayout, 3, 1, &voxelGridDescriptorSets[currentFrame], 0, nullptr);

	vkCmdDispatch(commandBuffer, voxelsPerSide / 8, voxelsPerSide / 8, voxelsPerSide / 8);
}

void Voxelizer::dispatchVoxelVisResetIndirectBufferComputeShader(VkCommandBuffer commandBuffer, uint32_t currentFrame)
{
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, voxelVisResetIndirectBufferComputePipeline);
	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, voxelVisResetIndirectBufferComputePipelineLayout, 0, 1, &voxelVisIndirectBufferDescriptorSet, 0, nullptr);

	vkCmdDispatch(commandBuffer, 1, 1, 1);
}