#include "Mesh.h"

#include <stdexcept>

Model::Model(std::string path, std::shared_ptr<Helper> helper) : 
    helper(helper), path(path), directory(path.substr(0, path.find_last_of('/')))
{
    createDescriptorSetLayouts(*helper);

    Assimp::Importer importer;

    const aiScene* scene = importer.ReadFile(path,
        aiProcess_CalcTangentSpace |
        aiProcess_Triangulate |
        aiProcess_JoinIdenticalVertices |
        aiProcess_SortByPType);

    if (nullptr == scene) {
        throw std::runtime_error("Failed to load model");
    }

    // Populate vertices and indices
    for (unsigned int i = 0; i < scene->mNumMeshes; i++) 
    {
		const aiMesh* mesh = scene->mMeshes[i];
        uint32_t materialIndex = mesh->mMaterialIndex;
        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;

        for (unsigned int j = 0; j < mesh->mNumVertices; j++) 
        {
			Vertex vertex = {};
			vertex.pos = glm::vec3(mesh->mVertices[j].x, mesh->mVertices[j].y, mesh->mVertices[j].z);
			vertex.normal = glm::vec3(mesh->mNormals[j].x, mesh->mNormals[j].y, mesh->mNormals[j].z);
            if (mesh->mTextureCoords[0]) {
				vertex.texCoord = mesh->HasTextureCoords(0) ? glm::vec2(mesh->mTextureCoords[0][j].x, 1-mesh->mTextureCoords[0][j].y) : glm::vec2(0.0f, 0.0f);
			}
			vertices.push_back(vertex);
		}

        for (unsigned int j = 0; j < mesh->mNumFaces; j++) 
        {
			const aiFace& face = mesh->mFaces[j];

            if (face.mNumIndices != 3)
            {
				throw std::runtime_error("Model not triangulated");
			}

            for (unsigned int k = 0; k < face.mNumIndices; k++) 
            {
				indices.push_back(face.mIndices[k]);
			}
		}

        meshes.emplace_back(std::make_unique<Mesh>(helper, std::move(vertices), std::move(indices), materialIndex));
	}

    // Populate materials
    textureImages.resize(scene->mNumMaterials);
    textureImagesMemory.resize(scene->mNumMaterials);
    textureImageViews.resize(scene->mNumMaterials);
    descriptorSets.resize(scene->mNumMaterials);
    mipLevels.resize(scene->mNumMaterials);

    helper->createSampler(textureSampler, 10);

    for (unsigned int i = 0; i < scene->mNumMaterials; i++)
    {
        const aiMaterial* material = scene->mMaterials[i];

        if (material->GetTextureCount(aiTextureType_DIFFUSE) > 0) 
        {
            aiString Path;

            if (material->GetTexture(aiTextureType_DIFFUSE, 0, &Path, NULL, NULL, NULL, NULL, NULL) == AI_SUCCESS) 
            {
                std::string FullPath = directory + "/" + Path.data;

                helper->createTextureImage(FullPath, textureImages[i], textureImagesMemory[i], textureImageViews[i], &mipLevels[i]);

                // Create descriptor set
                VkDescriptorSetLayout layouts[] = { getDescriptorSetLayout() };
                VkDescriptorSetAllocateInfo allocInfo = {};
                allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
                allocInfo.descriptorPool = helper->descriptorPool;
                allocInfo.descriptorSetCount = 1;
                allocInfo.pSetLayouts = layouts;

                if (vkAllocateDescriptorSets(helper->device, &allocInfo, &descriptorSets[i]) != VK_SUCCESS) {
					throw std::runtime_error("Failed to allocate descriptor sets");
				}

                VkDescriptorImageInfo imageInfo = {};
				imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				imageInfo.imageView = textureImageViews[i];
				imageInfo.sampler = textureSampler;

				VkWriteDescriptorSet descriptorWrite = {};
				descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				descriptorWrite.dstSet = descriptorSets[i];
				descriptorWrite.dstBinding = 0;
				descriptorWrite.dstArrayElement = 0;
				descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
				descriptorWrite.descriptorCount = 1;
				descriptorWrite.pImageInfo = &imageInfo;

				vkUpdateDescriptorSets(helper->device, 1, &descriptorWrite, 0, nullptr);
            }
            else
            {
                throw std::runtime_error("Texture path retrieval failed");
            }
        }
        else
        {
            std::cout << "Material has no diffuse texture\n";
            break;
        }
    }

}

Model::~Model()
{
    destroyDescriptorSetLayout(*helper);

    for (unsigned int i = 0; i < textureImages.size(); i++)
    {
        vkDestroyImageView(helper->device, textureImageViews[i], nullptr);
        vkFreeMemory(helper->device, textureImagesMemory[i], nullptr);
        vkDestroyImage(helper->device, textureImages[i], nullptr);
    }

    vkDestroySampler(helper->device, textureSampler, nullptr);
}

Mesh::Mesh(std::shared_ptr<Helper> helper, std::vector<Vertex>&& vertices, std::vector<uint32_t>&& indices, uint32_t materialIndex)
    : helper(helper), vertices(vertices), indices(indices), materialIndex(materialIndex)
{
    objectCount++;
    createVertexBuffer();
	createIndexBuffer();
    createVertexIndexDescriptorSetLayouts(*helper);
    createDescriptorSets();
}

Mesh::~Mesh()
{
    objectCount--;

    //std::cout << "Destroying mesh buffers\n";
    vkDestroyBuffer(helper->device, vertexBuffer, nullptr);
    vkFreeMemory(helper->device, vertexBufferMemory, nullptr);

    vkDestroyBuffer(helper->device, indexBuffer, nullptr);
    vkFreeMemory(helper->device, indexBufferMemory, nullptr);
}

void Mesh::createVertexBuffer()
{
    VkDeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    helper->createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

    void* data;
    vkMapMemory(helper->device, stagingBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, vertices.data(), (size_t)bufferSize);
    vkUnmapMemory(helper->device, stagingBufferMemory);

    helper->createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vertexBuffer, vertexBufferMemory);

    helper->copyBuffer(stagingBuffer, vertexBuffer, bufferSize);

    vkDestroyBuffer(helper->device, stagingBuffer, nullptr);
    vkFreeMemory(helper->device, stagingBufferMemory, nullptr);
}

void Mesh::createIndexBuffer()
{
    VkDeviceSize bufferSize = sizeof(indices[0]) * indices.size();

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    helper->createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

    void* data;
    vkMapMemory(helper->device, stagingBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, indices.data(), (size_t)bufferSize);
    vkUnmapMemory(helper->device, stagingBufferMemory);

    helper->createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, indexBuffer, indexBufferMemory);

    helper->copyBuffer(stagingBuffer, indexBuffer, bufferSize);

    vkDestroyBuffer(helper->device, stagingBuffer, nullptr);
    vkFreeMemory(helper->device, stagingBufferMemory, nullptr);
}

void Mesh::createDescriptorSets()
{
    VkDescriptorSetLayout layouts[] = { getVertexIndexDescriptorSetLayout() };
    VkDescriptorSetAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = helper->descriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = layouts;

    if (vkAllocateDescriptorSets(helper->device, &allocInfo, &vertexIndexBuffersDescriptorSet) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate descriptor sets");
    }

    VkDescriptorBufferInfo vertexBufferInfo = {};
    vertexBufferInfo.buffer = vertexBuffer;
    vertexBufferInfo.offset = 0;
    vertexBufferInfo.range = sizeof(vertices[0]) * vertices.size();

    VkDescriptorBufferInfo indexBufferInfo = {};
    indexBufferInfo.buffer = indexBuffer;
    indexBufferInfo.offset = 0;
    indexBufferInfo.range = sizeof(indices[0]) * indices.size();

    std::array<VkWriteDescriptorSet, 2> descriptorWrites = {};

    descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[0].dstSet = vertexIndexBuffersDescriptorSet;
    descriptorWrites[0].dstBinding = 0;
    descriptorWrites[0].dstArrayElement = 0;
    descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorWrites[0].descriptorCount = 1;
    descriptorWrites[0].pBufferInfo = &vertexBufferInfo;

    descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[1].dstSet = vertexIndexBuffersDescriptorSet;
    descriptorWrites[1].dstBinding = 1;
    descriptorWrites[1].dstArrayElement = 0;
    descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorWrites[1].descriptorCount = 1;
    descriptorWrites[1].pBufferInfo = &indexBufferInfo;

    vkUpdateDescriptorSets(helper->device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
}