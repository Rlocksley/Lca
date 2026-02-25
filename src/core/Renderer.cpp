#include "Renderer.hpp"
#include <map>
#include <cstring>
#include <algorithm>
#include "Queue.hpp"
#include "RenderPass.hpp"
#include "AssetManager.hpp"
#include "Window.hpp"
#include "Command.hpp"
#include "Swapchain.hpp"
#include "Framebuffers.hpp"
#include "PhysicalDevice.hpp"
namespace Lca{
    namespace Core{


 

        void Renderer::init(){
           
            for(uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
             
                objectInstancesGPU[i] = createDualBuffer(Lca::Core::MAX_OBJECTS, sizeof(ObjectInstance), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
                
                modelMatricesGPU[i] = createDualBuffer(Lca::Core::MAX_MODEL_MATRICES, sizeof(ModelMatrix), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
                
                lightsGPU[i] = createDualBuffer(Lca::Core::MAX_LIGHTS + 1, sizeof(Light), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

                cameraBuffer[i] = createDualBuffer(1, sizeof(Camera), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);

                drawCounts[i] = createBuffer(Lca::Core::MAX_SHADERS, sizeof(uint32_t), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT);
            }

            shaderCapacities = createDualBuffer(Lca::Core::MAX_SHADERS, sizeof(uint32_t), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
            memset(shaderCapacities.interface.pMemory, 0, Lca::Core::MAX_SHADERS * sizeof(uint32_t));
            dummyIndirectBuffer = createBuffer(Lca::Core::MAX_OBJECTS, sizeof(VkDrawIndexedIndirectCommand), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);

            uberCullPipeline = std::make_unique<UberCullPipeline>("shader/uber_cull.comp.spv");
            uberCullPipeline->build();

            GraphicsPipelineConfig depthPipelineConfig{};
            depthPipelineConfig.vertexShader = "shader/depth.vert.spv";
            
            depthPipelineConfig.depthBiasConstantFactor = 1.25f;
            depthPipelineConfig.depthBiasClamp = 0.0f;
            depthPipelineConfig.depthBiasSlopeFactor = 1.75f;

            depthPipelineConfig.sampleCount = VK_SAMPLE_COUNT_1_BIT;
            depthPipelineConfig.minSampleShading = 1.0f;

            depthPipelineConfig.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
            depthPipelineConfig.minDepthBounds = 0.0f;
            depthPipelineConfig.maxDepthBounds = 1.0f;

            depthPipelineConfig.hasColorAttachments = true;

            depthPipeline = std::make_unique<DepthPipeline>(depthPipelineConfig);
            depthPipeline->build();
        }

        void Renderer::shutdown(){
            uberCullPipeline.reset();
            depthPipeline.reset();

            for(uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
                destroyDualBuffer(objectInstancesGPU[i]);
                destroyDualBuffer(modelMatricesGPU[i]);
                destroyDualBuffer(lightsGPU[i]);
                destroyDualBuffer(cameraBuffer[i]);
                destroyBuffer(drawCounts[i]);
            }
            destroyDualBuffer(shaderCapacities);
            destroyBuffer(dummyIndirectBuffer);
            for(auto& buffer : indirectBuffers) {
                for(auto& buf : buffer) {
                    destroyBuffer(buf);
                }
                buffer.clear();
            }
            meshPipelineMap.clear();
            meshPipelines.clear();
        }

        void Renderer::updatePipelineDescriptorSets(){
            for (auto& meshPipeline : meshPipelines) {
                meshPipeline.updateDescriptorSetWrites();
            }
        }
        
        void Renderer::recordFrame(uint32_t frameIndex){
            getSwapchainImageIndex(frameIndex);

            beginCommand(command[frameIndex]);

            objectInstancesGPU[frameIndex].recordSync(command[frameIndex]);
            modelMatricesGPU[frameIndex].recordSync(command[frameIndex]);
            cameraBuffer[frameIndex].recordSync(command[frameIndex]);
            lightsGPU[frameIndex].recordSync(command[frameIndex]);
            GetAssetManager().recordSync(command[frameIndex]);

            vkCmdFillBuffer(command[frameIndex].vkCommandBuffer, drawCounts[frameIndex].vkBuffer, 0, VK_WHOLE_SIZE, 0);
            for (auto& indirectBuffer : indirectBuffers[frameIndex]) {
                vkCmdFillBuffer(command[frameIndex].vkCommandBuffer, indirectBuffer.vkBuffer, 0, VK_WHOLE_SIZE, 0);
            }

            VkMemoryBarrier transferToShaderBarrier{};
            transferToShaderBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
            transferToShaderBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            transferToShaderBarrier.dstAccessMask = VK_ACCESS_UNIFORM_READ_BIT |
                                                    VK_ACCESS_SHADER_READ_BIT |
                                                    VK_ACCESS_SHADER_WRITE_BIT;

            vkCmdPipelineBarrier(
                command[frameIndex].vkCommandBuffer,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT |
                VK_PIPELINE_STAGE_VERTEX_SHADER_BIT |
                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                0,
                1,
                &transferToShaderBarrier,
                0,
                nullptr,
                0,
                nullptr
            );

            {
                VkDescriptorSet descriptorSet = uberCullPipeline->getVkDescriptorSet(frameIndex);
                vkCmdBindPipeline(
                    command[frameIndex].vkCommandBuffer,
                    VK_PIPELINE_BIND_POINT_COMPUTE,
                    uberCullPipeline->getVkPipeline()
                );
                vkCmdBindDescriptorSets(
                    command[frameIndex].vkCommandBuffer,
                    VK_PIPELINE_BIND_POINT_COMPUTE,
                    uberCullPipeline->getVkPipelineLayout(),
                    0,
                    1,
                    &descriptorSet,
                    0,
                    nullptr
                );

                constexpr uint32_t WORKGROUP_SIZE_X = 256;
                if (objectInstances.getSize() > 0) {
                    const uint32_t groupCountX = (objectInstances.getSize() + WORKGROUP_SIZE_X - 1) / WORKGROUP_SIZE_X;
                    vkCmdDispatch(command[frameIndex].vkCommandBuffer, groupCountX, 1, 1);
                }
            }

            VkMemoryBarrier computeToDrawBarrier{};
            computeToDrawBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
            computeToDrawBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            computeToDrawBarrier.dstAccessMask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT;

            vkCmdPipelineBarrier(
                command[frameIndex].vkCommandBuffer,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
                0,
                1,
                &computeToDrawBarrier,
                0,
                nullptr,
                0,
                nullptr
            );

            // Use dynamic rendering (vkCmdBeginRendering / vkCmdEndRendering)
            {
                VkImage swapImage = swapchain.vkImages[swapchain.imageIndex];
                // Transition swapchain image from UNDEFINED to COLOR_ATTACHMENT_OPTIMAL before rendering
                VkImageMemoryBarrier swapBarrierInit{};
                swapBarrierInit.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                swapBarrierInit.srcAccessMask = 0;
                swapBarrierInit.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
                swapBarrierInit.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
                swapBarrierInit.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                swapBarrierInit.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                swapBarrierInit.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                swapBarrierInit.image = swapImage;
                swapBarrierInit.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                swapBarrierInit.subresourceRange.baseMipLevel = 0;
                swapBarrierInit.subresourceRange.levelCount = 1;
                swapBarrierInit.subresourceRange.baseArrayLayer = 0;
                swapBarrierInit.subresourceRange.layerCount = 1;

                vkCmdPipelineBarrier(
                    command[frameIndex].vkCommandBuffer,
                    VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                    0,
                    0, nullptr,
                    0, nullptr,
                    1, &swapBarrierInit
                );

                // Color attachment: use multisampled transient color image and resolve to swapchain view
                VkRenderingAttachmentInfo colorAttachment{};
                colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
                colorAttachment.imageView = framebuffers.colorImages[swapchain.imageIndex].vkImageView;
                colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
                colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
                colorAttachment.clearValue.color = {{0.0f, 0.0f, 0.0f, 1.0f}};
                colorAttachment.resolveMode = VK_RESOLVE_MODE_AVERAGE_BIT;
                colorAttachment.resolveImageView = swapchain.vkImageViews[swapchain.imageIndex];
                colorAttachment.resolveImageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

                // Depth attachment: per-frame transient depth image (must match sample count of colorAttachment)
                VkRenderingAttachmentInfo depthAttachment{};
                depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
                depthAttachment.imageView = framebuffers.depthImages[swapchain.imageIndex].vkImageView;
                depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
                depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
                depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
                depthAttachment.clearValue.depthStencil = {1.0f, 0};

                VkRenderingInfo renderingInfo{};
                renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
                renderingInfo.pNext = nullptr;
                renderingInfo.flags = 0;
                renderingInfo.renderArea.offset = {0, 0};
                renderingInfo.renderArea.extent = vkExtent2D;
                renderingInfo.layerCount = 1;
                renderingInfo.viewMask = 0;
                renderingInfo.colorAttachmentCount = 1;
                renderingInfo.pColorAttachments = &colorAttachment;
                renderingInfo.pDepthAttachment = &depthAttachment;
                renderingInfo.pStencilAttachment = nullptr;

                vkCmdBeginRendering(command[frameIndex].vkCommandBuffer, &renderingInfo);

                const Buffer vertexBuffer = GetAssetManager().getVertexBuffer();
                const Buffer indexBuffer = GetAssetManager().getIndexBuffer();

                VkBuffer vkVertexBuffer = vertexBuffer.vkBuffer;
                VkDeviceSize offsets[] = {0};
                vkCmdBindVertexBuffers(command[frameIndex].vkCommandBuffer, 0, 1, &vkVertexBuffer, offsets);
                vkCmdBindIndexBuffer(command[frameIndex].vkCommandBuffer, indexBuffer.vkBuffer, 0, VK_INDEX_TYPE_UINT32);

                const uint32_t meshPipelineCount = static_cast<uint32_t>(meshPipelines.size());
                LCA_ASSERT(meshPipelineCount == static_cast<uint32_t>(indirectBuffers[frameIndex].size()), "Renderer", "recordFrame", "meshPipelines and indirectBuffers size mismatch.");

                for (uint32_t i = 0; i < meshPipelineCount; ++i) {
                    MeshPipeline& meshPipeline = meshPipelines[i];

                    VkDescriptorSet descriptorSet = meshPipeline.getVkDescriptorSet(frameIndex);
                    vkCmdBindPipeline(
                        command[frameIndex].vkCommandBuffer,
                        VK_PIPELINE_BIND_POINT_GRAPHICS,
                        meshPipeline.getVkPipeline()
                    );
                    vkCmdBindDescriptorSets(
                        command[frameIndex].vkCommandBuffer,
                        VK_PIPELINE_BIND_POINT_GRAPHICS,
                        meshPipeline.getVkPipelineLayout(),
                        0,
                        1,
                        &descriptorSet,
                        0,
                        nullptr
                    );

                    const Buffer& indirectBuffer = indirectBuffers[frameIndex][i];
                    const VkDeviceSize drawCountOffset = static_cast<VkDeviceSize>(i) * sizeof(uint32_t);
                    vkCmdDrawIndexedIndirectCount(
                        command[frameIndex].vkCommandBuffer,
                        indirectBuffer.vkBuffer,
                        0,
                        drawCounts[frameIndex].vkBuffer,
                        drawCountOffset,
                        indirectBuffer.numberElements,
                        sizeof(VkDrawIndexedIndirectCommand)
                    );
                }

                vkCmdEndRendering(command[frameIndex].vkCommandBuffer);

                // Transition the swapchain image from COLOR_ATTACHMENT_OPTIMAL to PRESENT_SRC_KHR for presentation
                VkImageMemoryBarrier swapBarrierPresent{};
                swapBarrierPresent.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                swapBarrierPresent.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
                swapBarrierPresent.dstAccessMask = 0;
                swapBarrierPresent.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                swapBarrierPresent.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
                swapBarrierPresent.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                swapBarrierPresent.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                swapBarrierPresent.image = swapImage;
                swapBarrierPresent.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                swapBarrierPresent.subresourceRange.baseMipLevel = 0;
                swapBarrierPresent.subresourceRange.levelCount = 1;
                swapBarrierPresent.subresourceRange.baseArrayLayer = 0;
                swapBarrierPresent.subresourceRange.layerCount = 1;

                vkCmdPipelineBarrier(
                    command[frameIndex].vkCommandBuffer,
                    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                    VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                    0,
                    0, nullptr,
                    0, nullptr,
                    1, &swapBarrierPresent
                );
            }

            endCommand(command[frameIndex]);
         
        }


        void Renderer::submitFrame(uint32_t frameIndex){
            submitGraphicsCommand(command[frameIndex], frameIndex);
            presentGraphics(command[frameIndex], frameIndex);
        }

        uint32_t Renderer::addModelMatrix(const ModelMatrix& matrix){
            return modelMatrices.add(matrix);
        }

        

        void Renderer::removeModelMatrix(uint32_t id){
            modelMatrices.remove(id);
        }

        uint32_t Renderer::addObjectInstance(const ObjectInstance& instance){
            uint32_t id =  objectInstances.add(instance);
            return id;
        }

        void Renderer::updateObjectInstance(uint32_t id, const ObjectInstance& instance){
            objectInstances.update(id, instance);
        }

        void Renderer::removeObjectInstance(uint32_t id){
            objectInstances.remove(id);
        }
            
        uint32_t Renderer::addMeshPipeline(const std::string& name, MeshPipeline&& pipeline, uint32_t maxObjects){
            LCA_ASSERT(meshPipelineMap.find(name) == meshPipelineMap.end(), "Renderer", "addMeshPipeline", "Mesh pipeline name already exists.")
            LCA_ASSERT(meshPipelines.size() < MAX_SHADERS, "Renderer", "addMeshPipeline", "Exceeded MAX_SHADERS capacity.")
            LCA_ASSERT(maxObjects > 0, "Renderer", "addMeshPipeline", "maxObjects must be greater than 0.")

            meshPipelines.push_back(std::move(pipeline));
            const uint32_t id = static_cast<uint32_t>(meshPipelines.size() - 1);

            meshPipelines.back().build();
            meshPipelineMap[name] = id;

            for(uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
                Buffer buffer = createBuffer(maxObjects, sizeof(VkDrawIndexedIndirectCommand), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
                indirectBuffers[i].push_back(buffer);
            }

            auto* capacities = static_cast<uint32_t*>(shaderCapacities.interface.pMemory);
            capacities[id] = maxObjects;

            beginSingleCommand(singleCommand);
            shaderCapacities.recordSync(singleCommand);
            endSingleCommand(singleCommand);
            submitSingleCommand(singleCommand);
            
            uberCullPipeline->updateDescriptorSetWrites();

            return id;
        }

        uint32_t Renderer::getMeshPipelineId(const std::string& name) const {
            auto it = meshPipelineMap.find(name);
            LCA_ASSERT(it != meshPipelineMap.end(), "Renderer", "getMeshPipelineId", "Mesh pipeline name not found.")
            return it->second;
        }

        uint32_t Renderer::addLight(const Light& light){
            uint32_t id;
            if(!freeLightSlots.empty()){
                id = freeLightSlots.back();
                freeLightSlots.pop_back();
            } else {
                LCA_ASSERT(lightCount < MAX_LIGHTS, "Renderer", "addLight", "Exceeded MAX_LIGHTS capacity.")
                id = lightCount++;
            }
            lightSlots[id] = light;
            lightSlotActive[id] = true;
            lightsDirty = true;
            return id;
        }

        void Renderer::updateLight(uint32_t id, const Light& light){
            LCA_ASSERT(id < MAX_LIGHTS && lightSlotActive[id], "Renderer", "updateLight", "Invalid light ID.")
            lightSlots[id] = light;
            lightsDirty = true;
        }

        void Renderer::removeLight(uint32_t id){
            LCA_ASSERT(id < MAX_LIGHTS && lightSlotActive[id], "Renderer", "removeLight", "Invalid light ID.")
            lightSlotActive[id] = false;
            freeLightSlots.push_back(id);
            lightsDirty = true;
        }

        void Renderer::copyLightsToGPU(uint32_t frameIndex){
            // Layout matches the std430 SSBO in the fragment shader:
            //   uint lightCount;  // offset 0  (4 bytes)
            //   <12 bytes padding to align Light to 16>
            //   Light lights[];   // offset 16
            auto* mem = static_cast<uint8_t*>(lightsGPU[frameIndex].interface.pMemory);

            // Pack active lights densely after the 16-byte header.
            auto* dst = reinterpret_cast<Light*>(mem + 16);
            uint32_t packed = 0;
            for(uint32_t i = 0; i < lightCount; i++){
                if(lightSlotActive[i]){
                    dst[packed++] = lightSlots[i];
                }
            }
            // Zero remaining to avoid stale data
            if(packed < MAX_LIGHTS){
                memset(&dst[packed], 0, (MAX_LIGHTS - packed) * sizeof(Light));
            }

            // Write the packed count into the SSBO header (first 4 bytes).
            memcpy(mem, &packed, sizeof(uint32_t));
            // Zero the 12 padding bytes after lightCount.
            memset(mem + sizeof(uint32_t), 0, 12);

            packedLightCount = packed;
        }

        glm::mat4 Renderer::createViewMatrix(const glm::vec3& cameraPosition, const glm::vec3& cameraDirection, const glm::vec3& upDirection)
        {
           glm::vec3 cameraRight = glm::normalize(glm::cross(upDirection, cameraDirection));
            glm::vec3 cameraUp = glm::normalize(glm::cross(cameraRight, cameraDirection));

            glm::mat4 viewMatrix(1.0f);
            viewMatrix[0][0] = cameraRight.x;
            viewMatrix[0][1] = cameraRight.y;
            viewMatrix[0][2] = cameraRight.z;
            viewMatrix[1][0] = cameraUp.x;
            viewMatrix[1][1] = cameraUp.y;
            viewMatrix[1][2] = cameraUp.z;
            viewMatrix[2][0] = -cameraDirection.x;
            viewMatrix[2][1] = -cameraDirection.y;
            viewMatrix[2][2] = -cameraDirection.z;
            viewMatrix[3][0] = cameraPosition.x;
            viewMatrix[3][1] = cameraPosition.y;
            viewMatrix[3][2] = cameraPosition.z;

            viewMatrix = glm::inverse(viewMatrix);

            return viewMatrix;
        }

        glm::mat4 Renderer::createXMatrix()
        {
            glm::mat4 X = glm::mat4(1.f);
            X[1][1] = -1.f;
            X[2][2] = -1.f;
            return glm::inverse(X);
        }

        glm::mat4 Renderer::createProjectionMatrix(float fov, float aspectRatio, float nearClip, float farClip) 
        {
            glm::mat4 projectionMatrix(1.0f);

            float tanHalfFOV = tan(fov / 2.0f);
            float depthRangeInv = 1.0f / (farClip - nearClip);

            projectionMatrix[0][0] = 1.0f / (aspectRatio * tanHalfFOV);
            projectionMatrix[0][1] = 0.0f;
            projectionMatrix[0][2] = 0.0f;
            projectionMatrix[0][3] = 0.0f;
            projectionMatrix[1][0] = 0.0f;
            projectionMatrix[1][1] = 1.0f / tanHalfFOV;
            projectionMatrix[1][2] = 0.0f;
            projectionMatrix[1][3] = 0.0f;

            projectionMatrix[2][0] = 0.0f;
            projectionMatrix[2][1] = 0.0f;
            projectionMatrix[2][2] = farClip * depthRangeInv; // Vulkan clip depth [0, 1]
            projectionMatrix[2][3] = 1.0f;

            projectionMatrix[3][0] = 0.0f;
            projectionMatrix[3][1] = 0.0f;
            projectionMatrix[3][2] = (-farClip * nearClip) * depthRangeInv;
            projectionMatrix[3][3] = 0.0f;

            return projectionMatrix;
        }

        void Renderer::updateCamera(uint32_t frameIndex, glm::vec3 position, glm::vec3 direction, float fov, float nearClip, float farClip)
        {
            Camera cam{};
            cam.camPos = position;
            cam.camDir = glm::normalize(direction);

            cam.viewProjection = createProjectionMatrix(
                fov,
                static_cast<float>(windowWidth) / static_cast<float>(windowHeight),
                nearClip,
                farClip
            );
            cam.viewProjection *= createXMatrix();
            cam.viewProjection *= createViewMatrix(position, cam.camDir, glm::vec3(0.f, -1.f, 0.f));

            const glm::vec4 row0(
                cam.viewProjection[0][0],
                cam.viewProjection[1][0],
                cam.viewProjection[2][0],
                cam.viewProjection[3][0]
            );
            const glm::vec4 row1(
                cam.viewProjection[0][1],
                cam.viewProjection[1][1],
                cam.viewProjection[2][1],
                cam.viewProjection[3][1]
            );
            const glm::vec4 row2(
                cam.viewProjection[0][2],
                cam.viewProjection[1][2],
                cam.viewProjection[2][2],
                cam.viewProjection[3][2]
            );
            const glm::vec4 row3(
                cam.viewProjection[0][3],
                cam.viewProjection[1][3],
                cam.viewProjection[2][3],
                cam.viewProjection[3][3]
            );

            cam.frustumPlanes[0] = row3 + row0; // Left   :  x + w >= 0
            cam.frustumPlanes[1] = row3 - row0; // Right  : -x + w >= 0
            cam.frustumPlanes[2] = row3 + row1; // Bottom :  y + w >= 0
            cam.frustumPlanes[3] = row3 - row1; // Top    : -y + w >= 0
            cam.frustumPlanes[4] = row2;        // Near   :  z >= 0  (Vulkan depth [0,1])
            cam.frustumPlanes[5] = row3 - row2; // Far    : -z + w >= 0

            for (glm::vec4& plane : cam.frustumPlanes) {
                const float normalLength = glm::length(glm::vec3(plane));
                if (normalLength > 0.0f) {
                    plane /= normalLength;
                }
            }

            memcpy(cameraBuffer[frameIndex].interface.pMemory, &cam, sizeof(Camera));
        }
    }
}