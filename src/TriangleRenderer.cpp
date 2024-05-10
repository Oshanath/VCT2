#include "TriangleRenderer.h"

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <chrono>


TriangleRenderer::TriangleRenderer(std::string app_name) : Application(app_name), camera(std::make_shared<Camera>(glm::vec3(-2907.25, 2827.39, 755.888), glm::vec3(0.0f, 0.0f, 0.0f)))
{
    models.push_back(std::make_shared<Model>("models/sponza/Sponza.gltf", helper));
    renderObjects.push_back(std::make_shared<RenderObject>(helper, models[0]));

    lightUBO = std::make_shared<LightUBO>();
    lightUBO->direction = glm::vec4(0.3f, -1.0f, 0.3f, 1.0f);

    helper->camera = camera;

    shadowMap = std::make_unique<ShadowMap>(helper, lightUBO);
    voxelizer = std::make_shared<GeometryVoxelizer>(helper, 512, corner1, corner2);

    meshPushConstants.occlusionDecayFactor = 0.0f;
    meshPushConstants.ambientOcclusionEnabled = VK_FALSE;
    meshPushConstants.occlusionVisualizationEnabled = VK_FALSE;
    meshPushConstants.surfaceOffset = 15.719f;
    meshPushConstants.coneCutoff = 143.813f;

    createBuffers();
    createDescriptorSetLayouts();
    createDescriptorSets();
    createGraphicsPipeline();
}

void TriangleRenderer::cleanup_extended()
{
    for (auto& renderObject : renderObjects)
    {
        renderObject.reset();
	}

    for (auto& model : models)
    {
		model.reset();
	}

    shadowMap.reset();
    voxelizer.reset();

    vkDestroyPipeline(device, graphicsPipeline, nullptr);
    vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
    vkDestroyRenderPass(device, swapChainRenderPass, nullptr);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vkDestroyBuffer(device, transformationUniformBuffers[i], nullptr);
        vkFreeMemory(device, transformationUniformBuffersMemory[i], nullptr);

        vkDestroyBuffer(device, lightUniformBuffers[i], nullptr);
        vkFreeMemory(device, lightUniformBuffersMemory[i], nullptr);

        vkDestroyBuffer(device, lightSpaceMatrixUniformBuffers[i], nullptr);
        vkFreeMemory(device, lightSpaceMatrixUniformBuffersMemory[i], nullptr);
    }

    vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
}

void TriangleRenderer::createGraphicsPipeline()
{
    auto vertShaderCode = helper->readFile("shaders/main.vert.spv");
    auto fragShaderCode = helper->readFile("shaders/main.frag.spv");
    auto vertShaderModule = helper->createShaderModule(vertShaderCode);
    auto fragShaderModule = helper->createShaderModule(fragShaderCode);
    helper->setNameOfObject(VK_OBJECT_TYPE_SHADER_MODULE, (uint64_t)vertShaderModule, "TriangleRenderer::Vertex Shader Module");
    helper->setNameOfObject(VK_OBJECT_TYPE_SHADER_MODULE, (uint64_t)fragShaderModule, "TriangleRenderer::Fragment Shader Module");

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
    viewport.width = (float)swapChainExtent.width;
    viewport.height = (float)swapChainExtent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    // Scissor
    VkRect2D scissor{};
    scissor.offset = { 0, 0 };
    scissor.extent = swapChainExtent;

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
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
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

    // Push Constants
    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(MeshPushConstants);

    // Pipeline layout
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    std::vector<VkDescriptorSetLayout> layouts = { 
        descriptorSetLayout, 
        Model::descriptorSetLayout, 
        shadowMap->shadowMapDescriptorSetLayout, 
        voxelizer->mipMapperDescriptorSetLayout,
        voxelizer->voxelGridDescriptorSetLayout,
        voxelizer->noiseTextureDescriptorSetLayout
    };
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = layouts.size();
    pipelineLayoutInfo.pSetLayouts = layouts.data();
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

    if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
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
    pipelineInfo.layout = pipelineLayout;
    pipelineInfo.renderPass = swapChainRenderPass;
    pipelineInfo.subpass = 0;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE; // Optional
    pipelineInfo.basePipelineIndex = -1; // Optional

    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline) != VK_SUCCESS) {
        throw std::runtime_error("failed to create graphics pipeline!");
    }

    vkDestroyShaderModule(device, vertShaderModule, nullptr);
    vkDestroyShaderModule(device, fragShaderModule, nullptr);
}

void TriangleRenderer::renderScene()
{
    for (auto& renderObject : renderObjects)
    {
        meshPushConstants.model = renderObject->getModelMatrix();
        vkCmdPushConstants(commandBuffers[currentFrame], pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(MeshPushConstants), &meshPushConstants);

        for (auto& mesh : renderObject->model->meshes)
        {
            VkBuffer vertexBuffers[] = { mesh->vertexBuffer };
            VkDeviceSize offsets[] = { 0 };

            vkCmdBindVertexBuffers(commandBuffers[currentFrame], 0, 1, vertexBuffers, offsets);
            vkCmdBindIndexBuffer(commandBuffers[currentFrame], mesh->indexBuffer, 0, VK_INDEX_TYPE_UINT32);

            vkCmdBindDescriptorSets(commandBuffers[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSets[currentFrame], 0, nullptr);
            vkCmdBindDescriptorSets(commandBuffers[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 1, 1, &renderObject->model->descriptorSets[mesh->materialIndex], 0, nullptr);

            vkCmdDrawIndexed(commandBuffers[currentFrame], static_cast<uint32_t>(mesh->indices.size()), 1, 0, 0, 0);
        }
    }
}

void TriangleRenderer::recordCommandBuffer(uint32_t currentFrame, uint32_t imageIndex)
{
    VkImageMemoryBarrier imageMemoryBarrier{};
    imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    imageMemoryBarrier.image = voxelizer->voxelTexture;
    imageMemoryBarrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

    VkBufferMemoryBarrier indirectBufferMemoryBarrier{};
    indirectBufferMemoryBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    indirectBufferMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    indirectBufferMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    indirectBufferMemoryBarrier.buffer = voxelizer->indirectDrawBuffer;
    indirectBufferMemoryBarrier.offset = 0;
    indirectBufferMemoryBarrier.size = sizeof(VkDrawIndirectCommand);

    VkBufferMemoryBarrier instanceBufferMemoryBarrier{};
    instanceBufferMemoryBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    instanceBufferMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    instanceBufferMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    instanceBufferMemoryBarrier.buffer = voxelizer->instancePositionsBuffer;
    instanceBufferMemoryBarrier.offset = 0;
    instanceBufferMemoryBarrier.size = sizeof(glm::vec4) * voxelizer->INSTANCE_BUFFER_SIZE;

    VkMemoryBarrier memoryBarrier{};
    memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    memoryBarrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT;
    memoryBarrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;

    beginCommandBuffer();

    // Voxelization
    {
        voxelizer->updateUniformBuffers(currentFrame);

        vkCmdPipelineBarrier(commandBuffers[currentFrame], VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 1, &memoryBarrier, 0, nullptr, 0, nullptr);

        voxelizer->beginVoxelization(commandBuffers[currentFrame], currentFrame);
        for (auto& renderObject : renderObjects)
        {
            meshPushConstants.model = renderObject->getModelMatrix();
            vkCmdPushConstants(commandBuffers[currentFrame], pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(MeshPushConstants), &meshPushConstants);

            for (auto& mesh : renderObject->model->meshes)
            {
                VkBuffer vertexBuffers[] = { mesh->vertexBuffer };
                VkDeviceSize offsets[] = { 0 };

                vkCmdBindVertexBuffers(commandBuffers[currentFrame], 0, 1, vertexBuffers, offsets);
                vkCmdBindIndexBuffer(commandBuffers[currentFrame], mesh->indexBuffer, 0, VK_INDEX_TYPE_UINT32);

                std::shared_ptr<GeometryVoxelizer> vox = std::dynamic_pointer_cast<GeometryVoxelizer>(voxelizer);
                vkCmdBindDescriptorSets(commandBuffers[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, vox->voxelGridPipelineLayout, 1, 1, &renderObject->model->descriptorSets[mesh->materialIndex], 0, nullptr);

                vkCmdDrawIndexed(commandBuffers[currentFrame], static_cast<uint32_t>(mesh->indices.size()), 1, 0, 0, 0);
            }
        }
        voxelizer->endVoxelization(commandBuffers[currentFrame], currentFrame);

        vkCmdPipelineBarrier(commandBuffers[currentFrame], VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 1, &memoryBarrier, 0, nullptr, 0, nullptr);

        voxelizer->generateMipMaps(commandBuffers[currentFrame], currentFrame);

        vkCmdPipelineBarrier(commandBuffers[currentFrame], VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 1, &memoryBarrier, 0, nullptr, 0, nullptr);
    }


    // Voxel vis
    if (enableVoxelVis)
    {
        vkCmdPipelineBarrier(commandBuffers[currentFrame], VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 1, &memoryBarrier, 0, nullptr, 0, nullptr);

        voxelizer->dispatchVoxelVisResetIndirectBufferComputeShader(commandBuffers[currentFrame], currentFrame);

        vkCmdPipelineBarrier(commandBuffers[currentFrame], VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 1, &memoryBarrier, 0, nullptr, 0, nullptr);

        voxelizer->dispatchVoxelVisComputeShader(commandBuffers[currentFrame], currentFrame);

        vkCmdPipelineBarrier(commandBuffers[currentFrame], VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 1, &memoryBarrier, 0, nullptr, 0, nullptr);

        beginRenderPass(currentFrame, imageIndex);
        voxelizer->visualizeVoxelGrid(commandBuffers[currentFrame], currentFrame);

    }
    else
    {
        // Shadow map rendering
        shadowMap->beginRender(commandBuffers[currentFrame]);
        for (auto& renderObject : renderObjects)
        {
            glm::mat4 model = renderObject->getModelMatrix();
            vkCmdPushConstants(commandBuffers[currentFrame], shadowMap->pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &model);

            for (auto& mesh : renderObject->model->meshes)
            {
                VkBuffer vertexBuffers[] = { mesh->vertexBuffer };
                VkDeviceSize offsets[] = { 0 };

                vkCmdBindVertexBuffers(commandBuffers[currentFrame], 0, 1, vertexBuffers, offsets);
                vkCmdBindIndexBuffer(commandBuffers[currentFrame], mesh->indexBuffer, 0, VK_INDEX_TYPE_UINT32);

                vkCmdBindDescriptorSets(commandBuffers[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, shadowMap->pipelineLayout, 0, 1, &shadowMap->descriptorSet, 0, nullptr);

                vkCmdDrawIndexed(commandBuffers[currentFrame], static_cast<uint32_t>(mesh->indices.size()), 1, 0, 0, 0);
            }
        }
        shadowMap->endRender(commandBuffers[currentFrame]);

        // main rendering
        beginRenderPass(currentFrame, imageIndex);
        vkCmdBindPipeline(commandBuffers[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);
        vkCmdBindDescriptorSets(commandBuffers[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 2, 1, &shadowMap->shadowMapDescriptorSet, 0, nullptr);
        vkCmdBindDescriptorSets(commandBuffers[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 3, 1, &voxelizer->mipMapperDescriptorSet, 0, nullptr);
        vkCmdBindDescriptorSets(commandBuffers[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 4, 1, &voxelizer->voxelGridDescriptorSets[currentFrame], 0, nullptr);
        vkCmdBindDescriptorSets(commandBuffers[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 5, 1, &voxelizer->noiseTextureDescriptorSet, 0, nullptr);
        renderScene();
    }

    ImGui::Render();
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffers[currentFrame]);

    vkCmdEndRenderPass(commandBuffers[currentFrame]);    
    vkCmdPipelineBarrier(commandBuffers[currentFrame], VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 1, &memoryBarrier, 0, nullptr, 0, nullptr);

    if (vkEndCommandBuffer(commandBuffers[currentFrame]) != VK_SUCCESS) {
        throw std::runtime_error("failed to record command buffer!");
    }
}

void TriangleRenderer::beginRenderPass(uint32_t currentFrame, uint32_t imageIndex)
{
    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = swapChainRenderPass;
    renderPassInfo.framebuffer = swapChainFramebuffers[imageIndex];
    renderPassInfo.renderArea.offset = { 0, 0 };
    renderPassInfo.renderArea.extent = swapChainExtent;

    std::array<VkClearValue, 2> clearValues{};
    clearValues[0].color = { {0.0f, 0.0f, 0.0f, 1.0f} };
    clearValues[1].depthStencil = { 1.0f, 0 };

    renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    renderPassInfo.pClearValues = clearValues.data();

    vkCmdBeginRenderPass(commandBuffers[currentFrame], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    setDynamicState();

}

void TriangleRenderer::setDynamicState()
{
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(swapChainExtent.width);
    viewport.height = static_cast<float>(swapChainExtent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(commandBuffers[currentFrame], 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = { 0, 0 };
    scissor.extent = swapChainExtent;
    vkCmdSetScissor(commandBuffers[currentFrame], 0, 1, &scissor);
}

void TriangleRenderer::main_loop_extended(uint32_t currentFrame, uint32_t imageIndex)
{
    camera->deltaTime = deltaTime;
    camera->move();
    updateUniformBuffers(currentFrame);

    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    ImGui::Checkbox("Enable Voxel Visualization", &enableVoxelVis);

    ImGui::Text("");
    ImGui::Text("Voxel Grid resolution:");

    static int res_group = 3;
    if (ImGui::RadioButton("64", &res_group, 0))
        revoxelize(64);
    if (ImGui::RadioButton("128", &res_group, 1))
        revoxelize(128);
    if (ImGui::RadioButton("256", &res_group, 2))
        revoxelize(256);
    if (ImGui::RadioButton("512", &res_group, 3))
        revoxelize(512);

    ImGui::Text("");
    ImGui::Checkbox("Enable Ambient Occlusion", (bool*)&meshPushConstants.ambientOcclusionEnabled);
    ImGui::Checkbox("Enable Occlusion Visualization", (bool*) & meshPushConstants.occlusionVisualizationEnabled);
    ImGui::SliderFloat("Occlusion Decay Factor", &meshPushConstants.occlusionDecayFactor, 0.0f, 0.03f);
    ImGui::SliderFloat("Surface Offset", &meshPushConstants.surfaceOffset, 0.0f, 30.0f);
    ImGui::SliderFloat("Cone Cutoff", &meshPushConstants.coneCutoff, 0.0f, 2000.0f);


    recordCommandBuffer(currentFrame, imageIndex);
}

void TriangleRenderer::createBuffers()
{
    VkDeviceSize transformationMatricesBufferSize = sizeof(TransformationUniformBufferObject);
    VkDeviceSize lightBufferSize = sizeof(LightUBO);
    VkDeviceSize lightSpaceMatrixBufferSize = sizeof(LightSpaceMatrix);

    transformationUniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    transformationUniformBuffersMemory.resize(MAX_FRAMES_IN_FLIGHT);
    transformationUniformBuffersMapped.resize(MAX_FRAMES_IN_FLIGHT);

    lightUniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    lightUniformBuffersMemory.resize(MAX_FRAMES_IN_FLIGHT);
    lightUniformBuffersMapped.resize(MAX_FRAMES_IN_FLIGHT);

    lightSpaceMatrixUniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    lightSpaceMatrixUniformBuffersMemory.resize(MAX_FRAMES_IN_FLIGHT);
    lightSpaceMatrixUniformBuffersMapped.resize(MAX_FRAMES_IN_FLIGHT);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) 
    {
        helper->createBuffer(transformationMatricesBufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, transformationUniformBuffers[i], transformationUniformBuffersMemory[i]);
        vkMapMemory(device, transformationUniformBuffersMemory[i], 0, transformationMatricesBufferSize, 0, &transformationUniformBuffersMapped[i]);

        helper->createBuffer(lightBufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, lightUniformBuffers[i], lightUniformBuffersMemory[i]);
        vkMapMemory(device, lightUniformBuffersMemory[i], 0, lightBufferSize, 0, &lightUniformBuffersMapped[i]);

        helper->createBuffer(lightSpaceMatrixBufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, lightSpaceMatrixUniformBuffers[i], lightSpaceMatrixUniformBuffersMemory[i]);
        vkMapMemory(device, lightSpaceMatrixUniformBuffersMemory[i], 0, lightSpaceMatrixBufferSize, 0, &lightSpaceMatrixUniformBuffersMapped[i]);
    }
}

void TriangleRenderer::createDescriptorSetLayouts()
{
    // Transforms binding
    VkDescriptorSetLayoutBinding transformationUboLayoutBinding{};
    transformationUboLayoutBinding.binding = 0;
    transformationUboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    transformationUboLayoutBinding.descriptorCount = 1;
    transformationUboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    transformationUboLayoutBinding.pImmutableSamplers = nullptr; // Optional

    // Light binding
    VkDescriptorSetLayoutBinding lightUboLayoutBinding{};
    lightUboLayoutBinding.binding = 1;
    lightUboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    lightUboLayoutBinding.descriptorCount = 1;
    lightUboLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    lightUboLayoutBinding.pImmutableSamplers = nullptr; // Optional

    // Light space matrix binding
    VkDescriptorSetLayoutBinding lightSpaceMatrixUboLayoutBinding{};
    lightSpaceMatrixUboLayoutBinding.binding = 2;
    lightSpaceMatrixUboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    lightSpaceMatrixUboLayoutBinding.descriptorCount = 1;
    lightSpaceMatrixUboLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    lightSpaceMatrixUboLayoutBinding.pImmutableSamplers = nullptr; // Optional

    std::vector<VkDescriptorSetLayoutBinding> bindings = { transformationUboLayoutBinding, lightUboLayoutBinding, lightSpaceMatrixUboLayoutBinding };
    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();

    if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS) {
        throw std::runtime_error("failed to create descriptor set layout!");
    }
}

void TriangleRenderer::updateUniformBuffers(uint32_t currentImage)
{
    static auto startTime = std::chrono::high_resolution_clock::now();

    auto currentTime = std::chrono::high_resolution_clock::now();
    float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

    ViewProjectionMatrices matrices = camera->getViewProjectionMatrices(swapChainExtent.width, swapChainExtent.height);
    TransformationUniformBufferObject ubo{};
    ubo.view = matrices.view;
    ubo.projection = matrices.proj;
    ubo.cameraPosition = glm::vec4(camera->position, 1.0);

    LightSpaceMatrix lightSpaceMatrix;
    lightSpaceMatrix.model = shadowMap->getLightSpaceMatrix();

    memcpy(transformationUniformBuffersMapped[currentImage], &ubo, sizeof(ubo));
    memcpy(lightUniformBuffersMapped[currentImage], lightUBO.get(), sizeof(LightUBO));
    memcpy(lightSpaceMatrixUniformBuffersMapped[currentImage], &lightSpaceMatrix, sizeof(LightSpaceMatrix));
}

void TriangleRenderer::createDescriptorSets()
{
    // UBO
    std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, descriptorSetLayout);
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = descriptorPool;
    allocInfo.descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
    allocInfo.pSetLayouts = layouts.data();

    descriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
    if (vkAllocateDescriptorSets(device, &allocInfo, descriptorSets.data()) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate descriptor sets!");
    }

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) 
    {
        // Transformation UBO
        VkDescriptorBufferInfo transformsBufferInfo{};
        transformsBufferInfo.buffer = transformationUniformBuffers[i];
        transformsBufferInfo.offset = 0;
        transformsBufferInfo.range = sizeof(TransformationUniformBufferObject);

        std::array<VkWriteDescriptorSet, 3> descriptorWrites{};
        descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[0].dstSet = descriptorSets[i];
        descriptorWrites[0].dstBinding = 0;
        descriptorWrites[0].dstArrayElement = 0;
        descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrites[0].descriptorCount = 1;
        descriptorWrites[0].pBufferInfo = &transformsBufferInfo;

        // light UBO
        VkDescriptorBufferInfo lightBufferInfo{};
        lightBufferInfo.buffer = lightUniformBuffers[i];
        lightBufferInfo.offset = 0;
        lightBufferInfo.range = sizeof(LightUBO);

        descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[1].dstSet = descriptorSets[i];
        descriptorWrites[1].dstBinding = 1;
        descriptorWrites[1].dstArrayElement = 0;
        descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrites[1].descriptorCount = 1;
        descriptorWrites[1].pBufferInfo = &lightBufferInfo;

        // light space matrix UBO
        VkDescriptorBufferInfo lightSpaceMatrixBufferInfo{};
        lightSpaceMatrixBufferInfo.buffer = lightSpaceMatrixUniformBuffers[i];
        lightSpaceMatrixBufferInfo.offset = 0;
        lightSpaceMatrixBufferInfo.range = sizeof(LightSpaceMatrix);

        descriptorWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[2].dstSet = descriptorSets[i];
        descriptorWrites[2].dstBinding = 2;
        descriptorWrites[2].dstArrayElement = 0;
        descriptorWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrites[2].descriptorCount = 1;
        descriptorWrites[2].pBufferInfo = &lightSpaceMatrixBufferInfo;  

        vkUpdateDescriptorSets(device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
    }
}

void TriangleRenderer::key_callback_extended(GLFWwindow* window, int key, int scancode, int action, int mods, double deltaTime)
{
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
    {
        glfwSetWindowShouldClose(window, GLFW_TRUE);
    }

    if (key == GLFW_KEY_W && action == GLFW_PRESS)
    {
        camera->movingForward = true;
	}
    else if (key == GLFW_KEY_W && action == GLFW_RELEASE)
    {
        camera->movingForward = false;
    }

    if (key == GLFW_KEY_S && action == GLFW_PRESS)
    {
		camera->movingBackward = true;
	}
    else if (key == GLFW_KEY_S && action == GLFW_RELEASE)
    {
		camera->movingBackward = false;
	}

    if (key == GLFW_KEY_A && action == GLFW_PRESS)
    {
		camera->movingLeft = true;
	}
    else if (key == GLFW_KEY_A && action == GLFW_RELEASE)
    {
		camera->movingLeft = false;
	}

    if (key == GLFW_KEY_D && action == GLFW_PRESS)
    {
		camera->movingRight = true;
	}
    else if (key == GLFW_KEY_D && action == GLFW_RELEASE)
    {
		camera->movingRight = false;
	}

    if (key == GLFW_KEY_SPACE && action == GLFW_PRESS)
    {
		camera->movingUp = true;
	}
    else if (key == GLFW_KEY_SPACE && action == GLFW_RELEASE)
    {
		camera->movingUp = false;
	}

    if (key == GLFW_KEY_LEFT_CONTROL && action == GLFW_PRESS)
    {
		camera->movingDown = true;
	}
    else if (key == GLFW_KEY_LEFT_CONTROL && action == GLFW_RELEASE)
    {
		camera->movingDown = false;
	}

    if (key == GLFW_KEY_F && action == GLFW_PRESS)
    {
        std::cout << "Camera position: " << camera->position.x << ", " << camera->position.y << ", " << camera->position.z << std::endl;
	}
}

void TriangleRenderer::mouse_callback_extended(GLFWwindow* window, int button, int action, int mods, double deltaTime)
{
    if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS)
    {
		camera->freeLook = true;
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	}
    else if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_RELEASE)
    {
        camera->freeLook = false;
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
	}
}

void TriangleRenderer::cursor_position_callback_extended(GLFWwindow* window, double xpos, double ypos)
{
    camera->mouse_callback(xpos, ypos);
}

void TriangleRenderer::revoxelize(int resolution)
{
    if (resolution != voxelizer->voxelsPerSide)
    {
        vkDeviceWaitIdle(device);
        voxelizer.reset();
        vkDestroyPipeline(device, graphicsPipeline, nullptr);
        vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
        voxelizer = std::make_shared<GeometryVoxelizer>(helper, resolution, corner1, corner2);
        createGraphicsPipeline();
    }
}