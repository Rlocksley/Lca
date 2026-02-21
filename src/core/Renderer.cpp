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

namespace Lca{
    namespace Core{


 

        void Renderer::init(){
           
            for(uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
             
                objectInstancesGPU[i] = createDualBuffer(Lca::Core::MAX_OBJECTS, sizeof(ObjectInstance), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
                
                modelMatricesGPU[i] = createDualBuffer(Lca::Core::MAX_MODEL_MATRICES, sizeof(ModelMatrix), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
                
                cameraBuffer[i] = createDualBuffer(1, sizeof(Camera), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);

                drawCounts[i] = createBuffer(Lca::Core::MAX_SHADERS, sizeof(uint32_t), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT);
            }

            shaderCapacities = createDualBuffer(Lca::Core::MAX_SHADERS, sizeof(uint32_t), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
            memset(shaderCapacities.interface.pMemory, 0, Lca::Core::MAX_SHADERS * sizeof(uint32_t));
            dummyIndirectBuffer = createBuffer(Lca::Core::MAX_OBJECTS, sizeof(VkDrawIndexedIndirectCommand), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);

            uberCullPipeline = std::make_unique<UberCullPipeline>("shader/uber_cull.comp.spv");
            uberCullPipeline->build();
        }

        void Renderer::shutdown(){
            uberCullPipeline.reset();

            for(uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
                destroyDualBuffer(objectInstancesGPU[i]);
                destroyDualBuffer(modelMatricesGPU[i]);
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

            beginRenderPass(command[frameIndex]);
            {
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
            }
            endRenderPass(command[frameIndex]);

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