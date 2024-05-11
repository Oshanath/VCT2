#include "Voxelizer.h"

Voxelizer::Voxelizer(std::shared_ptr<Helper> helper, uint32_t voxelsPerSide, glm::vec4 corner1, glm::vec4 corner2, VoxelizationType type)	:
	helper(helper), voxelsPerSide(voxelsPerSide), aabbMin(glm::vec4(0.0f)), aabbMax(glm::vec4(0.0f)), center(glm::vec3(0.0f)), voxelizationType(type)
{
	mipLevelCount = static_cast<uint32_t>(std::floor(std::log2(voxelsPerSide))) + 1;

	helper->createImage(voxelsPerSide, voxelsPerSide, voxelsPerSide, mipLevelCount, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_STORAGE_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, voxelTexture, voxelTextureMemory);
	voxelTextureView = helper->createImageView(voxelTexture, 0, 1, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_VIEW_TYPE_3D);
	helper->setNameOfObject(VK_OBJECT_TYPE_IMAGE, (uint64_t)voxelTexture, "Voxel Texture");
	helper->setNameOfObject(VK_OBJECT_TYPE_IMAGE_VIEW, (uint64_t)voxelTextureView, "Voxel Texture View");

	helper->createTextureImage("models/noise.png", noiseTexture, noiseTextureMemory, noiseTextureView, nullptr);
	helper->createSampler(noiseTextureSampler, 1);

	// Create mip views
	voxelTextureMipViews.resize(mipLevelCount);
	for (uint32_t i = 0; i < mipLevelCount; i++)
	{
		voxelTextureMipViews[i] = helper->createImageView(voxelTexture, i, 1, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_VIEW_TYPE_3D);
		helper->setNameOfObject(VK_OBJECT_TYPE_IMAGE_VIEW, (uint64_t)voxelTextureMipViews[i], "Voxel Texture Mip View level" + std::to_string(i));
	}

	// Generate a unit cube from (0, 0, 0) to (1.0, 1.0, 1.0) in unitCubeVertices and unitCubeIndices
	unitCubeVertices = {
		{{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
		{{1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},
		{{1.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f}},
		{{0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 1.0f}},
		{{0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
		{{1.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},
		{{1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f}},
		{{0.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 1.0f}}
	};

	// subtract 0.5 from all vertex positions
	for (auto& vertex : unitCubeVertices)
	{
		vertex.pos -= glm::vec3(0.5f);
	}

	unitCubeIndices = {
		0, 1, 2, 2, 3, 0,
		1, 5, 6, 6, 2, 1,
		7, 6, 5, 5, 4, 7,
		4, 0, 3, 3, 7, 4,
		3, 2, 6, 6, 7, 3,
		4, 5, 1, 1, 0, 4
	};

	// Transition image layout
	VkImageMemoryBarrier barrier = {};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
	barrier.image = voxelTexture;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = mipLevelCount;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;
	barrier.srcAccessMask = 0;
	barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;

	VkCommandBuffer commandBuffer = helper->beginSingleTimeCommands();
	vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
	helper->endSingleTimeCommands(commandBuffer);

	calculateAABBMinMaxCenter(corner1, corner2);

	createBuffers();
	createCubeVertexindexBuffers();
	createVoxelVisResources();
	createDescriptorSetLayouts();
	createDescriptorSets();
	createVoxelVisComputePipeline();
	createVoxelVisResetIndirectBufferComputePipeline();
	createVoxelVisGraphicsPipeline();
	createMipMapperComputePipeline();
}

Voxelizer::~Voxelizer()
{
	vkDestroyDescriptorSetLayout(helper->device, voxelGridDescriptorSetLayout, nullptr);
	vkDestroyDescriptorSetLayout(helper->device, voxelTextureDescriptorSetLayout, nullptr);

	vkDestroyImageView(helper->device, voxelTextureView, nullptr);
	vkDestroyImage(helper->device, voxelTexture, nullptr);
	vkFreeMemory(helper->device, voxelTextureMemory, nullptr);

	for (auto mipView : voxelTextureMipViews)
	{
		vkDestroyImageView(helper->device, mipView, nullptr);
	}

	for (size_t i = 0; i < helper->MAX_FRAMES_IN_FLIGHT; i++)
	{
		vkDestroyBuffer(helper->device, voxelGridUniformBuffers[i], nullptr);
		vkFreeMemory(helper->device, voxelGridUniformBuffersMemory[i], nullptr);

		vkDestroyBuffer(helper->device, transformsUniformBuffers[i], nullptr);
		vkFreeMemory(helper->device, transformsUniformBuffersMemory[i], nullptr);

		vkDestroyBuffer(helper->device, cubeTransformsUniformBuffers[i], nullptr);
		vkFreeMemory(helper->device, cubeTransformsUniformBuffersMemory[i], nullptr);
	}

	vkDestroyDescriptorSetLayout(helper->device, voxelVisInstanceBufferDescriptorSetLayout, nullptr);
	vkDestroyDescriptorSetLayout(helper->device, voxelVisIndirectBufferDescriptorSetLayout, nullptr);
	vkDestroyPipelineLayout(helper->device, voxelVisComputePipelineLayout, nullptr);
	vkDestroyPipeline(helper->device, voxelVisComputePipeline, nullptr);

	vkDestroyBuffer(helper->device, instancePositionsBuffer, nullptr);
	vkFreeMemory(helper->device, instancePositionsBufferMemory, nullptr);
	vkDestroyBuffer(helper->device, instanceColorsBuffer, nullptr);
	vkFreeMemory(helper->device, instanceColorsBufferMemory, nullptr);
	vkDestroyBuffer(helper->device, indirectDrawBuffer, nullptr);
	vkFreeMemory(helper->device, indirectDrawBufferMemory, nullptr);
	vkDestroyBuffer(helper->device, unitCubeVertexBuffer, nullptr);
	vkFreeMemory(helper->device, unitCubeVertexBufferMemory, nullptr);
	vkDestroyBuffer(helper->device, unitCubeIndexBuffer, nullptr);
	vkFreeMemory(helper->device, unitCubeIndexBufferMemory, nullptr);


	vkDestroyPipelineLayout(helper->device, voxelVisResetIndirectBufferComputePipelineLayout, nullptr);
	vkDestroyPipeline(helper->device, voxelVisResetIndirectBufferComputePipeline, nullptr);

	vkDestroyDescriptorSetLayout(helper->device, voxelVisCubeTransformsUBODescriptorSetLayout, nullptr);

	vkDestroyPipelineLayout(helper->device, voxelVisGraphicsPipelineLayout, nullptr);
	vkDestroyPipeline(helper->device, voxelVisGraphicsPipeline, nullptr);

	vkDestroyDescriptorSetLayout(helper->device, mipMapperDescriptorSetLayout, nullptr);

	vkDestroyPipelineLayout(helper->device, mipMapperComputePipelineLayout, nullptr);
	vkDestroyPipeline(helper->device, mipMapperComputePipeline, nullptr);

	vkDestroyBuffer(helper->device, mipMapperAtomicCountersBuffer, nullptr);
	vkFreeMemory(helper->device, mipMapperAtomicCountersBufferMemory, nullptr);

	vkDestroyImageView(helper->device, noiseTextureView, nullptr);
	vkDestroyImage(helper->device, noiseTexture, nullptr);
	vkFreeMemory(helper->device, noiseTextureMemory, nullptr);
	vkDestroySampler(helper->device, noiseTextureSampler, nullptr);

	vkDestroyDescriptorSetLayout(helper->device, noiseTextureDescriptorSetLayout, nullptr);
}

void Voxelizer::calculateAABBMinMaxCenter(glm::vec4 corner1, glm::vec4 corner2)
{
	glm::vec4 center4 = (corner1 + corner2) / 2.0f;
	float longestSide = std::max(corner2.x - corner1.x, std::max(corner2.y - corner1.y, corner2.z - corner1.z));
	center = glm::vec3(center4);
	length = longestSide;
	voxelWidth = longestSide / voxelsPerSide;

	aabbMin = glm::vec4(center - glm::vec3(longestSide / 2.0f), 0.0f);
	aabbMax = glm::vec4(center + glm::vec3(longestSide / 2.0f), 0.0f);
}

void Voxelizer::createBuffers()
{
	VkDeviceSize bufferSize = sizeof(VoxelGridUBO);
	VkDeviceSize transformsBufferSize = sizeof(ViewProjectionMatrices);
	VkDeviceSize cubeTransformsBufferSize = sizeof(CubeModelViewProjectionMatrices);

	voxelGridUniformBuffers.resize(helper->MAX_FRAMES_IN_FLIGHT);
	voxelGridUniformBuffersMemory.resize(helper->MAX_FRAMES_IN_FLIGHT);
	voxelGridUniformBuffersMapped.resize(helper->MAX_FRAMES_IN_FLIGHT);

	transformsUniformBuffers.resize(helper->MAX_FRAMES_IN_FLIGHT);
	transformsUniformBuffersMemory.resize(helper->MAX_FRAMES_IN_FLIGHT);
	transformsUniformBuffersMapped.resize(helper->MAX_FRAMES_IN_FLIGHT);

	cubeTransformsUniformBuffers.resize(helper->MAX_FRAMES_IN_FLIGHT);
	cubeTransformsUniformBuffersMemory.resize(helper->MAX_FRAMES_IN_FLIGHT);
	cubeTransformsUniformBuffersMapped.resize(helper->MAX_FRAMES_IN_FLIGHT);

	for (size_t i = 0; i < helper->MAX_FRAMES_IN_FLIGHT; i++)
	{
		helper->createBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, voxelGridUniformBuffers[i], voxelGridUniformBuffersMemory[i]);
		vkMapMemory(helper->device, voxelGridUniformBuffersMemory[i], 0, bufferSize, 0, &voxelGridUniformBuffersMapped[i]);

		helper->createBuffer(transformsBufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, transformsUniformBuffers[i], transformsUniformBuffersMemory[i]);
		vkMapMemory(helper->device, transformsUniformBuffersMemory[i], 0, transformsBufferSize, 0, &transformsUniformBuffersMapped[i]);

		helper->createBuffer(cubeTransformsBufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, cubeTransformsUniformBuffers[i], cubeTransformsUniformBuffersMemory[i]);
		vkMapMemory(helper->device, cubeTransformsUniformBuffersMemory[i], 0, cubeTransformsBufferSize, 0, &cubeTransformsUniformBuffersMapped[i]);
	}

	// create atomic counters buffers
	helper->createBuffer(sizeof(uint32_t) * mipLevelCount, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, mipMapperAtomicCountersBuffer, mipMapperAtomicCountersBufferMemory);
}

void Voxelizer::createCubeVertexindexBuffers()
{
	// Vertex buffer
	{
		VkDeviceSize vertexBufferSize = sizeof(unitCubeVertices[0]) * unitCubeVertices.size();

		VkBuffer stagingBuffer;
		VkDeviceMemory stagingBufferMemory;
		helper->createBuffer(vertexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

		void* data;
		vkMapMemory(helper->device, stagingBufferMemory, 0, vertexBufferSize, 0, &data);
		memcpy(data, unitCubeVertices.data(), (size_t)vertexBufferSize);
		vkUnmapMemory(helper->device, stagingBufferMemory);

		helper->createBuffer(vertexBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, unitCubeVertexBuffer, unitCubeVertexBufferMemory);

		helper->copyBuffer(stagingBuffer, unitCubeVertexBuffer, vertexBufferSize);

		vkDestroyBuffer(helper->device, stagingBuffer, nullptr);
		vkFreeMemory(helper->device, stagingBufferMemory, nullptr);
	}

	// Index buffer
	{
		VkDeviceSize bufferSize = sizeof(unitCubeIndices[0]) * unitCubeIndices.size();

		VkBuffer stagingBuffer;
		VkDeviceMemory stagingBufferMemory;
		helper->createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

		void* data;
		vkMapMemory(helper->device, stagingBufferMemory, 0, bufferSize, 0, &data);
		memcpy(data, unitCubeIndices.data(), (size_t)bufferSize);
		vkUnmapMemory(helper->device, stagingBufferMemory);

		helper->createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, unitCubeIndexBuffer, unitCubeIndexBufferMemory);

		helper->copyBuffer(stagingBuffer, unitCubeIndexBuffer, bufferSize);

		vkDestroyBuffer(helper->device, stagingBuffer, nullptr);
		vkFreeMemory(helper->device, stagingBufferMemory, nullptr);
	}
}

ViewProjectionMatrices Voxelizer::getViewProjectionMatrices()
{
	ViewProjectionMatrices vp = {};

	glm::vec3 min = glm::vec3(aabbMin);
	glm::vec3 max = glm::vec3(aabbMax);
	float length = max.x - min.x;

	glm::vec3 eye = min + glm::vec3(length / 2.0f, length / 2.0f, 0.0f);
	vp.view = glm::lookAt(eye, center, glm::vec3(0.0f, 1.0f, 0.0f));
	vp.proj = glm::ortho(-length / 2.0f, length / 2.0f, -length / 2.0f, length / 2.0f, 0.0f, length);

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
	// Voxelization UBO layout
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

	// Voxel texture layout
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

	// Mip mapper
	VkDescriptorSetLayoutBinding mipMapperImageViewsLayoutBinding = {};
	mipMapperImageViewsLayoutBinding.binding = 0;
	mipMapperImageViewsLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	mipMapperImageViewsLayoutBinding.descriptorCount = mipLevelCount;
	mipMapperImageViewsLayoutBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
	mipMapperImageViewsLayoutBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutBinding mipMapperAtomicCountersLayoutBinding = {};
	mipMapperAtomicCountersLayoutBinding.binding = 1;
	mipMapperAtomicCountersLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	mipMapperAtomicCountersLayoutBinding.descriptorCount = 1;
	mipMapperAtomicCountersLayoutBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;	
	mipMapperAtomicCountersLayoutBinding.pImmutableSamplers = nullptr;

	std::vector<VkDescriptorSetLayoutBinding> bindings3 = { mipMapperImageViewsLayoutBinding, mipMapperAtomicCountersLayoutBinding };

	VkDescriptorSetLayoutCreateInfo layoutInfo3 = {};
	layoutInfo3.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo3.bindingCount = static_cast<uint32_t>(bindings3.size());
	layoutInfo3.pBindings = bindings3.data();

	if (vkCreateDescriptorSetLayout(helper->device, &layoutInfo3, nullptr, &mipMapperDescriptorSetLayout) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create descriptor set layout!");
	}

	// Noise texture
	VkDescriptorSetLayoutBinding noiseTextureLayoutBinding = {};
	noiseTextureLayoutBinding.binding = 0;
	noiseTextureLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	noiseTextureLayoutBinding.descriptorCount = 1;
	noiseTextureLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	noiseTextureLayoutBinding.pImmutableSamplers = nullptr;

	std::vector<VkDescriptorSetLayoutBinding> bindings4 = { noiseTextureLayoutBinding };

	VkDescriptorSetLayoutCreateInfo layoutInfo4 = {};
	layoutInfo4.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo4.bindingCount = static_cast<uint32_t>(bindings4.size());
	layoutInfo4.pBindings = bindings4.data();

	if (vkCreateDescriptorSetLayout(helper->device, &layoutInfo4, nullptr, &noiseTextureDescriptorSetLayout) != VK_SUCCESS)
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

	// Mip mapper
	VkDescriptorSetAllocateInfo allocInfo3 = {};
	allocInfo3.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo3.descriptorPool = helper->descriptorPool;
	allocInfo3.descriptorSetCount = 1;
	allocInfo3.pSetLayouts = &mipMapperDescriptorSetLayout;

	if (vkAllocateDescriptorSets(helper->device, &allocInfo3, &mipMapperDescriptorSet) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to allocate descriptor sets!");
	}

	std::vector<VkDescriptorImageInfo> mipMapperImageViews;
	for (uint32_t i = 0; i < mipLevelCount; i++)
	{
		VkDescriptorImageInfo mipMapperImageViewInfo = {};
		mipMapperImageViewInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		mipMapperImageViewInfo.imageView = voxelTextureMipViews[i];
		mipMapperImageViewInfo.sampler = VK_NULL_HANDLE;
		mipMapperImageViews.push_back(mipMapperImageViewInfo);
	}

	VkDescriptorBufferInfo mipMapperAtomicCounterBufferInfo = {};
	mipMapperAtomicCounterBufferInfo.buffer = mipMapperAtomicCountersBuffer;
	mipMapperAtomicCounterBufferInfo.offset = 0;
	mipMapperAtomicCounterBufferInfo.range = sizeof(uint32_t) * mipLevelCount;

	VkWriteDescriptorSet descriptorWrites2 = {};
	std::vector<VkDescriptorImageInfo> imageInfos;

	for (uint32_t i = 0; i < mipLevelCount; i++)
	{
		VkDescriptorImageInfo imageInfo = {};
		imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		imageInfo.imageView = voxelTextureMipViews[i];
		imageInfo.sampler = VK_NULL_HANDLE;

		imageInfos.push_back(imageInfo);
	}

	VkWriteDescriptorSet mipMapperImageViewsDescriptorWrite = {};
	mipMapperImageViewsDescriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	mipMapperImageViewsDescriptorWrite.dstSet = mipMapperDescriptorSet;
	mipMapperImageViewsDescriptorWrite.dstBinding = 0;
	mipMapperImageViewsDescriptorWrite.dstArrayElement = 0;
	mipMapperImageViewsDescriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	mipMapperImageViewsDescriptorWrite.descriptorCount = mipLevelCount;
	mipMapperImageViewsDescriptorWrite.pImageInfo = imageInfos.data();

	VkWriteDescriptorSet mipMapperAtomicCountersDescriptorWrite = {};
	mipMapperAtomicCountersDescriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	mipMapperAtomicCountersDescriptorWrite.dstSet = mipMapperDescriptorSet;
	mipMapperAtomicCountersDescriptorWrite.dstBinding = 1;
	mipMapperAtomicCountersDescriptorWrite.dstArrayElement = 0;
	mipMapperAtomicCountersDescriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	mipMapperAtomicCountersDescriptorWrite.descriptorCount = 1;
	mipMapperAtomicCountersDescriptorWrite.pBufferInfo = &mipMapperAtomicCounterBufferInfo;

	std::vector<VkWriteDescriptorSet> descriptorWrites3 = { mipMapperImageViewsDescriptorWrite, mipMapperAtomicCountersDescriptorWrite };

	vkUpdateDescriptorSets(helper->device, static_cast<uint32_t>(descriptorWrites3.size()), descriptorWrites3.data(), 0, nullptr);

	// Noise texture
	VkDescriptorSetAllocateInfo allocInfo4 = {};
	allocInfo4.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo4.descriptorPool = helper->descriptorPool;
	allocInfo4.descriptorSetCount = 1;
	allocInfo4.pSetLayouts = &noiseTextureDescriptorSetLayout;

	if (vkAllocateDescriptorSets(helper->device, &allocInfo4, &noiseTextureDescriptorSet) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to allocate descriptor sets!");
	}

	VkDescriptorImageInfo noiseTextureImageInfo = {};
	noiseTextureImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	noiseTextureImageInfo.imageView = noiseTextureView;
	noiseTextureImageInfo.sampler = noiseTextureSampler;

	VkWriteDescriptorSet noiseTextureDescriptorWrite = {};
	noiseTextureDescriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	noiseTextureDescriptorWrite.dstSet = noiseTextureDescriptorSet;
	noiseTextureDescriptorWrite.dstBinding = 0;
	noiseTextureDescriptorWrite.dstArrayElement = 0;
	noiseTextureDescriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	noiseTextureDescriptorWrite.descriptorCount = 1;
	noiseTextureDescriptorWrite.pImageInfo = &noiseTextureImageInfo;

	vkUpdateDescriptorSets(helper->device, 1, &noiseTextureDescriptorWrite, 0, nullptr);

}

void Voxelizer::createVoxelVisResources()
{
	VkDeviceSize instancePositionsBufferSize = sizeof(glm::vec4) * INSTANCE_BUFFER_SIZE;
	VkDeviceSize instanceColorsBufferSize = sizeof(glm::vec4) * INSTANCE_BUFFER_SIZE;
	VkDeviceSize indirectDrawBufferSize = sizeof(VkDrawIndexedIndirectCommand);

	helper->createBuffer(instancePositionsBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, instancePositionsBuffer, instancePositionsBufferMemory);
	helper->createBuffer(instanceColorsBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, instanceColorsBuffer, instanceColorsBufferMemory);
	helper->createBuffer(indirectDrawBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, indirectDrawBuffer, indirectDrawBufferMemory);

	// Indirect buffer contents
	VkDrawIndexedIndirectCommand indirectDrawCommand = {};
	indirectDrawCommand.indexCount = 36;
	indirectDrawCommand.instanceCount = 0;
	indirectDrawCommand.firstIndex = 0;
	indirectDrawCommand.vertexOffset = 0;
	indirectDrawCommand.firstInstance = 0;

	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;
	helper->createBuffer(indirectDrawBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);
	void* data;
	vkMapMemory(helper->device, stagingBufferMemory, 0, indirectDrawBufferSize, 0, &data);
	memcpy(data, &indirectDrawCommand, (size_t)indirectDrawBufferSize);
	vkUnmapMemory(helper->device, stagingBufferMemory);
	helper->copyBuffer(stagingBuffer, indirectDrawBuffer, indirectDrawBufferSize);
	vkDestroyBuffer(helper->device, stagingBuffer, nullptr);
	vkFreeMemory(helper->device, stagingBufferMemory, nullptr);

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

	// Voxel vis cube transforms layout
	VkDescriptorSetLayoutBinding cubeTransformsLayoutBinding = {};
	cubeTransformsLayoutBinding.binding = 0;
	cubeTransformsLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	cubeTransformsLayoutBinding.descriptorCount = 1;
	cubeTransformsLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	cubeTransformsLayoutBinding.pImmutableSamplers = nullptr;

	std::vector<VkDescriptorSetLayoutBinding> bindings3 = { cubeTransformsLayoutBinding };

	VkDescriptorSetLayoutCreateInfo layoutInfo3 = {};
	layoutInfo3.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo3.bindingCount = static_cast<uint32_t>(bindings3.size());
	layoutInfo3.pBindings = bindings3.data();

	if (vkCreateDescriptorSetLayout(helper->device, &layoutInfo3, nullptr, &voxelVisCubeTransformsUBODescriptorSetLayout) != VK_SUCCESS)
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

	// Cube transforms UBO
	VkDescriptorSetAllocateInfo allocInfo3 = {};
	allocInfo3.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo3.descriptorPool = helper->descriptorPool;
	allocInfo3.descriptorSetCount = 1;
	allocInfo3.pSetLayouts = &voxelVisCubeTransformsUBODescriptorSetLayout;

	if (vkAllocateDescriptorSets(helper->device, &allocInfo3, &voxelVisCubeTransformsUBODescriptorSet) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to allocate descriptor sets!");
	}

	VkDescriptorBufferInfo cubeTransformsBufferInfo = {};
	cubeTransformsBufferInfo.buffer = cubeTransformsUniformBuffers[0];
	cubeTransformsBufferInfo.offset = 0;
	cubeTransformsBufferInfo.range = sizeof(CubeModelViewProjectionMatrices);

	VkWriteDescriptorSet cubeTransformsDescriptorWrite = {};
	cubeTransformsDescriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	cubeTransformsDescriptorWrite.dstSet = voxelVisCubeTransformsUBODescriptorSet;
	cubeTransformsDescriptorWrite.dstBinding = 0;
	cubeTransformsDescriptorWrite.dstArrayElement = 0;
	cubeTransformsDescriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	cubeTransformsDescriptorWrite.descriptorCount = 1;
	cubeTransformsDescriptorWrite.pBufferInfo = &cubeTransformsBufferInfo;

	vkUpdateDescriptorSets(helper->device, 1, &cubeTransformsDescriptorWrite, 0, nullptr);
}

void Voxelizer::createVoxelVisComputePipeline()
{
	// Load compute shader pipeline
	auto computeShaderCode = helper->readFile("shaders/voxelVisPerVoxel.comp.spv");
	auto voxelVisComputeShaderModule = helper->createShaderModule(computeShaderCode);

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

	vkDestroyShaderModule(helper->device, voxelVisComputeShaderModule, nullptr);
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

void Voxelizer::createVoxelVisGraphicsPipeline()
{
	auto vertShaderCode = helper->readFile("shaders/voxelVis.vert.spv");
	auto fragShaderCode = helper->readFile("shaders/voxelVis.frag.spv");
	auto vertShaderModule = helper->createShaderModule(vertShaderCode);
	auto fragShaderModule = helper->createShaderModule(fragShaderCode);
	helper->setNameOfObject(VK_OBJECT_TYPE_SHADER_MODULE, (uint64_t)vertShaderModule, "Voxelizer::Vertex Shader Module");
	helper->setNameOfObject(VK_OBJECT_TYPE_SHADER_MODULE, (uint64_t)fragShaderModule, "Voxelizer::Fragment Shader Module");

	// Vertex shader stage
	VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
	vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertShaderStageInfo.module = vertShaderModule;
	vertShaderStageInfo.pName = "main";

	// Fragment shader stage
	VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
	fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragShaderStageInfo.module = fragShaderModule;
	fragShaderStageInfo.pName = "main";
	VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

	// Depth stencil state
	VkPipelineDepthStencilStateCreateInfo depthStencil{};
	depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencil.depthTestEnable = VK_TRUE;
	depthStencil.depthWriteEnable = VK_TRUE;
	depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
	depthStencil.depthBoundsTestEnable = VK_FALSE;
	depthStencil.minDepthBounds = 0.0f; // Optional
	depthStencil.maxDepthBounds = 1.0f; // Optional
	depthStencil.stencilTestEnable = VK_FALSE;
	depthStencil.front = {}; // Optional
	depthStencil.back = {}; // Optional

	// Vertex input
	VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputInfo.vertexBindingDescriptionCount = 1;
	vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(Vertex::getAttributeDescriptions().size());
	VkVertexInputBindingDescription bindingDescription = Vertex::getBindingDescription();
	vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
	vertexInputInfo.pVertexAttributeDescriptions = Vertex::getAttributeDescriptions().data();

	// Input assembly
	VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
	inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssembly.primitiveRestartEnable = VK_FALSE;

	// Viewport
	VkViewport viewport{};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = (float)helper->swapChainExtent.width;
	viewport.height = (float)helper->swapChainExtent.height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	// Scissor
	VkRect2D scissor{};
	scissor.offset = { 0, 0 };
	scissor.extent = helper->swapChainExtent;

	// Dynamic state
	std::vector<VkDynamicState> dynamicStates = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
	VkPipelineDynamicStateCreateInfo dynamicState{};
	dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
	dynamicState.pDynamicStates = dynamicStates.data();

	// Viewport state
	VkPipelineViewportStateCreateInfo viewportState{};
	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.viewportCount = 1;
	viewportState.scissorCount = 1;

	// Rasterizer
	VkPipelineRasterizationStateCreateInfo rasterizer{};
	rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer.depthClampEnable = VK_FALSE;
	rasterizer.rasterizerDiscardEnable = VK_FALSE;
	rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizer.lineWidth = 1.0f;
	rasterizer.cullMode = VK_CULL_MODE_NONE;
	rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rasterizer.depthBiasEnable = VK_FALSE;
	rasterizer.depthBiasConstantFactor = 0.0f; // Optional
	rasterizer.depthBiasClamp = 0.0f; // Optional
	rasterizer.depthBiasSlopeFactor = 0.0f; // Optional

	// Multisampling
	VkPipelineMultisampleStateCreateInfo multisampling{};
	multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.sampleShadingEnable = VK_FALSE;
	multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	multisampling.minSampleShading = 1.0f; // Optional
	multisampling.pSampleMask = nullptr; // Optional
	multisampling.alphaToCoverageEnable = VK_FALSE; // Optional
	multisampling.alphaToOneEnable = VK_FALSE; // Optional

	// Color blending
	VkPipelineColorBlendAttachmentState colorBlendAttachment{};
	colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	colorBlendAttachment.blendEnable = VK_TRUE;
	colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
	colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

	VkPipelineColorBlendStateCreateInfo colorBlending{};
	colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlending.logicOpEnable = VK_FALSE;
	colorBlending.logicOp = VK_LOGIC_OP_COPY; // Optional
	colorBlending.attachmentCount = 1;
	colorBlending.pAttachments = &colorBlendAttachment;
	colorBlending.blendConstants[0] = 0.0f; // Optional
	colorBlending.blendConstants[1] = 0.0f; // Optional
	colorBlending.blendConstants[2] = 0.0f; // Optional
	colorBlending.blendConstants[3] = 0.0f; // Optional

	// Pipeline layout
	VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
	std::vector<VkDescriptorSetLayout> layouts = { voxelVisCubeTransformsUBODescriptorSetLayout, voxelVisInstanceBufferDescriptorSetLayout };
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = layouts.size();
	pipelineLayoutInfo.pSetLayouts = layouts.data();
	pipelineLayoutInfo.pushConstantRangeCount = 0;
	pipelineLayoutInfo.pPushConstantRanges = nullptr;

	if (vkCreatePipelineLayout(helper->device, &pipelineLayoutInfo, nullptr, &voxelVisGraphicsPipelineLayout) != VK_SUCCESS) {
		throw std::runtime_error("failed to create pipeline layout!");
	}

	VkGraphicsPipelineCreateInfo pipelineInfo{};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineInfo.stageCount = 2;
	pipelineInfo.pStages = shaderStages;
	pipelineInfo.pVertexInputState = &vertexInputInfo;
	pipelineInfo.pInputAssemblyState = &inputAssembly;
	pipelineInfo.pViewportState = &viewportState;
	pipelineInfo.pRasterizationState = &rasterizer;
	pipelineInfo.pMultisampleState = &multisampling;
	pipelineInfo.pDepthStencilState = &depthStencil;
	pipelineInfo.pColorBlendState = &colorBlending;
	pipelineInfo.pDynamicState = &dynamicState;
	pipelineInfo.layout = voxelVisGraphicsPipelineLayout;
	pipelineInfo.renderPass = helper->swapChainRenderPass;
	pipelineInfo.subpass = 0;
	pipelineInfo.basePipelineHandle = VK_NULL_HANDLE; // Optional
	pipelineInfo.basePipelineIndex = -1; // Optional

	if (vkCreateGraphicsPipelines(helper->device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &voxelVisGraphicsPipeline) != VK_SUCCESS) {
		throw std::runtime_error("failed to create graphics pipeline!");
	}

	vkDestroyShaderModule(helper->device, vertShaderModule, nullptr);
	vkDestroyShaderModule(helper->device, fragShaderModule, nullptr);
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

void Voxelizer::visualizeVoxelGrid(VkCommandBuffer commandBuffer, uint32_t currentFrame)
{
	// Update uniform buffer for voxel vis
	CubeModelViewProjectionMatrices uboVis{};
	ViewProjectionMatrices transforms{};
	transforms = helper->camera->getViewProjectionMatrices(helper->swapChainExtent.width, helper->swapChainExtent.height);
	uboVis.projection = transforms.proj;
	uboVis.view = transforms.view;
	uboVis.model = glm::scale(glm::mat4(1.0f), glm::vec3(voxelWidth * 2));
	memcpy(cubeTransformsUniformBuffersMapped[currentFrame], &uboVis, sizeof(uboVis));

	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, voxelVisGraphicsPipeline);
	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, voxelVisGraphicsPipelineLayout, 0, 1, &voxelVisCubeTransformsUBODescriptorSet, 0, nullptr);
	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, voxelVisGraphicsPipelineLayout, 1, 1, &voxelVisInstanceBufferDescriptorSet, 0, nullptr);

	VkBuffer vertexBuffers[] = { unitCubeVertexBuffer };
	VkDeviceSize offsets[] = { 0 };

	vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
	vkCmdBindIndexBuffer(commandBuffer, unitCubeIndexBuffer, 0, VK_INDEX_TYPE_UINT32);

	vkCmdDrawIndexedIndirect(commandBuffer, indirectDrawBuffer, 0, 1, sizeof(VkDrawIndexedIndirectCommand));
}

void Voxelizer::createMipMapperComputePipeline()
{
	auto computeShaderCode = helper->readFile("shaders/MipMapper.comp.spv");
	VkShaderModule computeShaderModule = helper->createShaderModule(computeShaderCode);

	VkPipelineShaderStageCreateInfo computeShaderStageInfo = {};
	computeShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	computeShaderStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	computeShaderStageInfo.module = computeShaderModule;
	computeShaderStageInfo.pName = "main";

	VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = 1;
	pipelineLayoutInfo.pSetLayouts = &mipMapperDescriptorSetLayout;
	pipelineLayoutInfo.pushConstantRangeCount = 0;
	pipelineLayoutInfo.pPushConstantRanges = nullptr;

	if (vkCreatePipelineLayout(helper->device, &pipelineLayoutInfo, nullptr, &mipMapperComputePipelineLayout) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create pipeline layout!");
	}

	VkComputePipelineCreateInfo pipelineInfo = {};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	pipelineInfo.stage = computeShaderStageInfo;
	pipelineInfo.layout = mipMapperComputePipelineLayout;

	if (vkCreateComputePipelines(helper->device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &mipMapperComputePipeline) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create graphics pipeline!");
	}

	vkDestroyShaderModule(helper->device, computeShaderModule, nullptr);
}

void Voxelizer::generateMipMaps(VkCommandBuffer commandBuffer, uint32_t currentFrame)
{
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, mipMapperComputePipeline);
	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, mipMapperComputePipelineLayout, 0, 1, &mipMapperDescriptorSet, 0, nullptr);

	vkCmdDispatch(commandBuffer, voxelsPerSide / 8, voxelsPerSide / 8, voxelsPerSide / 8);
}