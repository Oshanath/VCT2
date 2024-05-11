#include "ComputeVoxelizer.h"

ComputeVoxelizer::ComputeVoxelizer(std::shared_ptr<Helper> helper, uint32_t voxelsPerSide, glm::vec4 corner1, glm::vec4 corner2) :
	Voxelizer(helper, voxelsPerSide, corner1, corner2, COMPUTE_SHADER_VOXELIZATION)
{
	createBuffers();
	createDescriptorSetLayouts();
	createDescriptorSets();
	createSmallTrianglesComputePipeline();
}

ComputeVoxelizer::~ComputeVoxelizer()
{
	vkDestroyBuffer(helper->device, indirectDispatchBuffer, nullptr);
	vkFreeMemory(helper->device, indirectDispatchBufferMemory, nullptr);
	vkDestroyDescriptorSetLayout(helper->device, indirectDispatchBufferDescriptorSetLayout, nullptr);
}

void ComputeVoxelizer::createBuffers()
{
	VkDeviceSize indirectDispatchBufferSize = sizeof(VkDispatchIndirectCommand);
	helper->createBuffer(indirectDispatchBufferSize, VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, indirectDispatchBuffer, indirectDispatchBufferMemory);

	VkDeviceSize largeTriangleBufferSize = sizeof(LargeTriangle) * LARGE_TRIANGLE_BUFFER_SIZE;
	helper->createBuffer(largeTriangleBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, largeTriangleBuffer, largeTriangleBufferMemory);
}

void ComputeVoxelizer::createDescriptorSetLayouts()
{
	// Indirect dispatch buffer
	VkDescriptorSetLayoutBinding indirectDispatchBufferLayoutBinding = {};
	indirectDispatchBufferLayoutBinding.binding = 0;
	indirectDispatchBufferLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	indirectDispatchBufferLayoutBinding.descriptorCount = 1;
	indirectDispatchBufferLayoutBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

	std::array<VkDescriptorSetLayoutBinding, 1> bindings = { indirectDispatchBufferLayoutBinding };

	VkDescriptorSetLayoutCreateInfo layoutInfo = {};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
	layoutInfo.pBindings = bindings.data();

	if (vkCreateDescriptorSetLayout(helper->device, &layoutInfo, nullptr, &indirectDispatchBufferDescriptorSetLayout) != VK_SUCCESS) {
		throw std::runtime_error("failed to create descriptor set layout!");
	}

	// Large triangle buffer
	VkDescriptorSetLayoutBinding largeTriangleBufferLayoutBinding = {};
	largeTriangleBufferLayoutBinding.binding = 0;
	largeTriangleBufferLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	largeTriangleBufferLayoutBinding.descriptorCount = 1;
	largeTriangleBufferLayoutBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

	std::array<VkDescriptorSetLayoutBinding, 1> largeTriangleBufferBindings = { largeTriangleBufferLayoutBinding };

	VkDescriptorSetLayoutCreateInfo largeTriangleBufferLayoutInfo = {};
	largeTriangleBufferLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	largeTriangleBufferLayoutInfo.bindingCount = static_cast<uint32_t>(largeTriangleBufferBindings.size());

	if (vkCreateDescriptorSetLayout(helper->device, &largeTriangleBufferLayoutInfo, nullptr, &largeTriangleBufferDescriptorSetLayout) != VK_SUCCESS) {
		throw std::runtime_error("failed to create descriptor set layout!");
	}
}

void ComputeVoxelizer::createDescriptorSets()
{
	// Indirect dispatch buffer
	VkDescriptorSetLayout layouts[] = { indirectDispatchBufferDescriptorSetLayout };
	VkDescriptorSetAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = helper->descriptorPool;
	allocInfo.descriptorSetCount = 1;
	allocInfo.pSetLayouts = layouts;

	if (vkAllocateDescriptorSets(helper->device, &allocInfo, &indirectDispatchBufferDescriptorSet) != VK_SUCCESS) {
		throw std::runtime_error("failed to allocate descriptor sets!");
	}

	VkDescriptorBufferInfo indirectDispatchBufferInfo = {};
	indirectDispatchBufferInfo.buffer = indirectDispatchBuffer;
	indirectDispatchBufferInfo.offset = 0;
	indirectDispatchBufferInfo.range = sizeof(VkDispatchIndirectCommand);

	std::array<VkWriteDescriptorSet, 1> descriptorWrites = {};

	descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descriptorWrites[0].dstSet = indirectDispatchBufferDescriptorSet;
	descriptorWrites[0].dstBinding = 0;
	descriptorWrites[0].dstArrayElement = 0;
	descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	descriptorWrites[0].descriptorCount = 1;
	descriptorWrites[0].pBufferInfo = &indirectDispatchBufferInfo;

	vkUpdateDescriptorSets(helper->device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);

	// Large triangle buffer
	VkDescriptorSetLayout largeTriangleBufferLayouts[] = { largeTriangleBufferDescriptorSetLayout };
	VkDescriptorSetAllocateInfo largeTriangleBufferAllocInfo = {};
	largeTriangleBufferAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	largeTriangleBufferAllocInfo.descriptorPool = helper->descriptorPool;
	largeTriangleBufferAllocInfo.descriptorSetCount = 1;
	largeTriangleBufferAllocInfo.pSetLayouts = largeTriangleBufferLayouts;

	if (vkAllocateDescriptorSets(helper->device, &largeTriangleBufferAllocInfo, &largeTriangleBufferDescriptorSet) != VK_SUCCESS) {
		throw std::runtime_error("failed to allocate descriptor sets!");
	}

	VkDescriptorBufferInfo largeTriangleBufferInfo = {};
	largeTriangleBufferInfo.buffer = largeTriangleBuffer;
	largeTriangleBufferInfo.offset = 0;
	largeTriangleBufferInfo.range = sizeof(LargeTriangle) * LARGE_TRIANGLE_BUFFER_SIZE;

	std::array<VkWriteDescriptorSet, 1> largeTriangleDescriptorWrites = {};

	largeTriangleDescriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	largeTriangleDescriptorWrites[0].dstSet = largeTriangleBufferDescriptorSet;
	largeTriangleDescriptorWrites[0].dstBinding = 0;
	largeTriangleDescriptorWrites[0].dstArrayElement = 0;
	largeTriangleDescriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	largeTriangleDescriptorWrites[0].descriptorCount = 1;
	largeTriangleDescriptorWrites[0].pBufferInfo = &largeTriangleBufferInfo;

	vkUpdateDescriptorSets(helper->device, static_cast<uint32_t>(largeTriangleDescriptorWrites.size()), largeTriangleDescriptorWrites.data(), 0, nullptr);
}

void ComputeVoxelizer::createSmallTrianglesComputePipeline()
{
	// Load compute shader pipeline
	auto computeShaderCode = helper->readFile("shaders/smallTriangles.comp.spv");
	auto voxelVisComputeShaderModule = helper->createShaderModule(computeShaderCode);

	VkPipelineShaderStageCreateInfo computeShaderStageInfo = {};
	computeShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	computeShaderStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	computeShaderStageInfo.module = voxelVisComputeShaderModule;
	computeShaderStageInfo.pName = "main";

	std::vector<VkDescriptorSetLayout> layouts = { 
		voxelTextureDescriptorSetLayout, 
		voxelGridDescriptorSetLayout, 
		Mesh::vertexIndexBufferDescriptorSetLayout, 
		Model::descriptorSetLayout,
		indirectDispatchBufferDescriptorSetLayout,
		largeTriangleBufferDescriptorSetLayout
	};

	VkPushConstantRange pushConstantRange = {};
	pushConstantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	pushConstantRange.offset = 0;
	pushConstantRange.size = sizeof(computeVoxelizationPushConstants);

	VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = layouts.size();
	pipelineLayoutInfo.pSetLayouts = layouts.data();
	pipelineLayoutInfo.pushConstantRangeCount = 1;
	pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

	if (vkCreatePipelineLayout(helper->device, &pipelineLayoutInfo, nullptr, &smallTrianglesComputePipelineLayout) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create pipeline layout!");
	}

	VkComputePipelineCreateInfo pipelineInfo = {};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	pipelineInfo.stage = computeShaderStageInfo;
	pipelineInfo.layout = smallTrianglesComputePipelineLayout;

	if (vkCreateComputePipelines(helper->device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &smallTrianglesComputePipeline) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create graphics pipeline!");
	}

	vkDestroyShaderModule(helper->device, voxelVisComputeShaderModule, nullptr);
}

void ComputeVoxelizer::beginVoxelization(VkCommandBuffer commandBuffer, uint32_t currentFrame)
{
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, smallTrianglesComputePipeline);
	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, smallTrianglesComputePipelineLayout, 0, 1, &voxelTextureDescriptorSet, 0, nullptr);
	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, smallTrianglesComputePipelineLayout, 1, 1, &voxelGridDescriptorSets[currentFrame], 0, nullptr);
	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, smallTrianglesComputePipelineLayout, 4, 1, &indirectDispatchBufferDescriptorSet, 0, nullptr);
	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, smallTrianglesComputePipelineLayout, 5, 1, &largeTriangleBufferDescriptorSet, 0, nullptr);
}