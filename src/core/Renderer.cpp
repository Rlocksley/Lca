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
            
                // Per-frame single-sample depth map used by compute light culling
                depthMaps[i] = createDepthMap(vkExtent2D.width, vkExtent2D.height);
                // Per-frame depth pre-pass indirect buffer and its count
                depthPrePassBuffer[i] = createBuffer(Lca::Core::MAX_OBJECTS, sizeof(VkDrawIndexedIndirectCommand), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
                depthPrePassCountBuffer[i] = createBuffer(1, sizeof(uint32_t), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT);

                // Create per-frame tile light index buffers for the light culling compute shader.
                // Must match shader constants
                const uint32_t tilesX = (vkExtent2D.width + TILE_WIDTH - 1u) / TILE_WIDTH;
                const uint32_t tilesY = (vkExtent2D.height + TILE_HEIGHT - 1u) / TILE_HEIGHT;
                const uint32_t entriesPerTile = MAX_LIGHTS_PER_TILE + 1u; // count + indices
                const uint32_t totalElements = tilesX * tilesY * entriesPerTile;
                lightIndicesBuffer[i] = createBuffer(totalElements, sizeof(uint32_t), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);

                // ── Skeleton mesh rendering buffers ───────────────────
                skeletonMeshInstancesGPU[i] = createDualBuffer(Lca::Core::MAX_OBJECTS, sizeof(SkeletonMeshInstance), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
                skeletonInstancesGPU[i] = createDualBuffer(Lca::Core::MAX_SKELETON_INSTANCES, sizeof(SkeletonInstance), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
                skeletonDrawCounts[i] = createBuffer(Lca::Core::MAX_SHADERS, sizeof(uint32_t), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT);
                skeletonDepthPrePassBuffer[i] = createBuffer(Lca::Core::MAX_OBJECTS, sizeof(VkDrawIndexedIndirectCommand), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
                skeletonDepthPrePassCountBuffer[i] = createBuffer(1, sizeof(uint32_t), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT);

                // ── Particle system rendering buffers ─────────────────
                particleSystemInstancesGPU[i] = createDualBuffer(Lca::Core::MAX_PARTICLE_SYSTEMS, sizeof(ParticleSystemInstance), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
                particleDeltaTimeBuffer[i]     = createDualBuffer(1, sizeof(ParticleDeltaTimeUniform), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
                particleGfxDrawCounts[i] = createBuffer(Lca::Core::MAX_PARTICLE_GFX_PIPELINES, sizeof(uint32_t),
                    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT);
            }

            // Per-pipeline device-local indirect buffers written every frame by ParticleCullPipeline.
            // particleGfxIndirectBuffers[graphicsID][slot]: VkDrawIndexedIndirectCommand
            //   firstInstance = particleOffset, instanceCount = particleCount (0 when culled).
            // Indexed by global particle system slot; buffers are created in addParticleSystemPipeline().
            // One VkDispatchIndirectCommand per comp pipeline (not per system).
            // y and z are always 1; only x (workgroup count) varies and is written by the cull shader.
            // The CPU resets x to 0 each frame via vkCmdUpdateBuffer before the cull dispatch.
            particleCompIndirectBuffer = createBuffer(
                Lca::Core::MAX_PARTICLE_COMP_PIPELINES, sizeof(VkDispatchIndirectCommand),
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
            // Dispatch table: maps global workgroup ID -> (particleBase, systemSlot).
            // Written by the cull shader; read by the sim shader via gl_WorkGroupID.x.
            particleDispatchTableBuffer = createBuffer(
                Lca::Core::MAX_PARTICLE_DISPATCH_ENTRIES, sizeof(ParticleDispatchEntry),
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);

            // One large device-local particle storage buffer shared by all systems.
            particleStorageBuffer = createBuffer(Lca::Core::MAX_PARTICLES, sizeof(Particle),
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
            currentParticleTop = 0;
            particleFreeRanges.clear();

            // If comp pipelines were registered before init(), initialise their y=1, z=1 now.
            shaderCapacities = createDualBuffer(Lca::Core::MAX_SHADERS, sizeof(uint32_t), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
            memset(shaderCapacities.interface.pMemory, 0, Lca::Core::MAX_SHADERS * sizeof(uint32_t));
            dummyIndirectBuffer = createBuffer(Lca::Core::MAX_OBJECTS, sizeof(VkDrawIndexedIndirectCommand), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);

            skeletonShaderCapacities = createDualBuffer(Lca::Core::MAX_SHADERS, sizeof(uint32_t), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
            memset(skeletonShaderCapacities.interface.pMemory, 0, Lca::Core::MAX_SHADERS * sizeof(uint32_t));
            skeletonDummyIndirectBuffer = createBuffer(Lca::Core::MAX_OBJECTS, sizeof(VkDrawIndexedIndirectCommand), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);

            // Fill GPU-side instance buffers with 0xFFFFFFFF so every
            // uninitialised slot has transformID == UINT32_MAX and is
            // rejected by the cull shaders' invalid-slot check.
            // Without this, the 250 zero-filled slots in the last
            // dispatch workgroup pass the check and generate ghost draw
            // commands, destroying framerate.
            beginSingleCommand(singleCommand);
            for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
                memset(objectInstancesGPU[i].interface.pMemory, 0xFF,
                       MAX_OBJECTS * sizeof(ObjectInstance));
                vkCmdFillBuffer(singleCommand.vkCommandBuffer,
                                objectInstancesGPU[i].buffer.vkBuffer,
                                0, VK_WHOLE_SIZE, 0xFFFFFFFF);

                memset(skeletonMeshInstancesGPU[i].interface.pMemory, 0xFF,
                       MAX_OBJECTS * sizeof(SkeletonMeshInstance));
                vkCmdFillBuffer(singleCommand.vkCommandBuffer,
                                skeletonMeshInstancesGPU[i].buffer.vkBuffer,
                                0, VK_WHOLE_SIZE, 0xFFFFFFFF);
            }
            endSingleCommand(singleCommand);
            submitSingleCommand(singleCommand);

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

            depthPipelineConfig.hasColorAttachments = false;

            depthPipeline = std::make_unique<DepthPipeline>(depthPipelineConfig);
            depthPipeline->build();

            // Skeleton depth pipeline: same config but with a skinning depth shader
            GraphicsPipelineConfig skeletonDepthConfig = depthPipelineConfig;
            skeletonDepthConfig.vertexShader = "shader/skeleton_depth.vert.spv";
            skeletonDepthPipeline = std::make_unique<SkeletonDepthPipeline>(skeletonDepthConfig);
            skeletonDepthPipeline->build();

            skeletonCullPipeline = std::make_unique<SkeletonCullPipeline>("shader/skeleton_cull.comp.spv");
            skeletonCullPipeline->build();

            // Pre-reserve so that addParticleSystemPipeline() / addMeshPipeline() never
            // trigger a reallocation.  Pipeline has no user-defined move constructor, so a
            // reallocation would copy the raw VkPipeline handle then destroy the old element,
            // calling vkDestroyPipeline on a still-live handle (use-after-free / 0xee handle).
            particlePipelines.reserve(Lca::Core::MAX_PARTICLE_GFX_PIPELINES);
            meshPipelines.reserve(Lca::Core::MAX_SHADERS);
            skeletonMeshPipelines.reserve(Lca::Core::MAX_SHADERS);

            // Particle cull pipeline: built after all particle pipelines are registered
            // (same pattern as uberCull). The shader path placeholder will be replaced
            // once particle_cull.comp.spv is compiled.
            particleCullPipeline = std::make_unique<ParticleCullPipeline>("shader/particle_cull.comp.spv");
            particleCullPipeline->build();
            
            lightCullPipeline = std::make_unique<LightCullPipeline>("shader/light_cull.comp.spv");
            lightCullPipeline->build();
        }

        void Renderer::shutdown(){
            particleCullPipeline.reset();
            uberCullPipeline.reset();
            skeletonCullPipeline.reset();
            depthPipeline.reset();
            skeletonDepthPipeline.reset();
            lightCullPipeline.reset();

            for(uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
                destroyDualBuffer(objectInstancesGPU[i]);
                destroyDualBuffer(modelMatricesGPU[i]);
                destroyDualBuffer(lightsGPU[i]);
                destroyDualBuffer(cameraBuffer[i]);
                destroyBuffer(drawCounts[i]);
                destroyTexture(depthMaps[i]);
                destroyBuffer(lightIndicesBuffer[i]);
                destroyBuffer(depthPrePassBuffer[i]);
                destroyBuffer(depthPrePassCountBuffer[i]);

                destroyDualBuffer(skeletonMeshInstancesGPU[i]);
                destroyDualBuffer(skeletonInstancesGPU[i]);
                destroyBuffer(skeletonDrawCounts[i]);
                destroyBuffer(skeletonDepthPrePassBuffer[i]);
                destroyBuffer(skeletonDepthPrePassCountBuffer[i]);

                destroyDualBuffer(particleSystemInstancesGPU[i]);
                destroyDualBuffer(particleDeltaTimeBuffer[i]);
                destroyBuffer(particleGfxDrawCounts[i]);
            }
            destroyBuffer(particleStorageBuffer);
            for (auto& buf : particleGfxIndirectBuffers) { destroyBuffer(buf); }
            particleGfxIndirectBuffers.clear();
            destroyBuffer(particleCompIndirectBuffer);
            destroyBuffer(particleDispatchTableBuffer);

            particlePipelines.clear();
            particlePipelineMap.clear();
            particleGfxSystems.clear();
            particleCompPipelines.clear();
            particleCompPipelineMap.clear();
            particleCompSystems.clear();
            destroyDualBuffer(shaderCapacities);
            destroyBuffer(dummyIndirectBuffer);

            destroyDualBuffer(skeletonShaderCapacities);
            destroyBuffer(skeletonDummyIndirectBuffer);

            for(auto& buffer : indirectBuffers) {
                for(auto& buf : buffer) {
                    destroyBuffer(buf);
                }
                buffer.clear();
            }

            for(auto& buffer : skeletonIndirectBuffers) {
                for(auto& buf : buffer) {
                    destroyBuffer(buf);
                }
                buffer.clear();
            }

            meshPipelineMap.clear();
            meshPipelines.clear();

            skeletonMeshPipelineMap.clear();
            skeletonMeshPipelines.clear();
        }

        void Renderer::updatePipelineDescriptorSets(){
            for (auto& meshPipeline : meshPipelines) {
                meshPipeline.updateDescriptorSetWrites();
            }
            for (auto& skeletonPipeline : skeletonMeshPipelines) {
                skeletonPipeline.updateDescriptorSetWrites();
            }
        }
        
        void Renderer::recordFrame(uint32_t frameIndex){
            getSwapchainImageIndex(frameIndex);

            beginCommand(command[frameIndex]);

            // --- Dynamic block: upload only the slots that changed this frame.
            // copyModelMatricesToGPU / copyObjectInstancesToGPU (called by ECS
            // systems before recordFrame) already wrote only dirty elements to
            // the staging buffers and returned their indices.  Static entities
            // (Component::Static) stop producing dirty indices after their
            // first MAX_FRAMES_IN_FLIGHT frames, so they never incur a GPU
            // copy again.  Dynamic entities are re-marked dirty every frame by
            // the "Non Static Transform Update" system and always appear here.
            objectInstancesGPU[frameIndex].recordSyncRanges(command[frameIndex], dirtyObjectInstanceIndices[frameIndex]);
            modelMatricesGPU[frameIndex].recordSyncRanges(command[frameIndex], dirtyModelMatrixIndices[frameIndex]);
            cameraBuffer[frameIndex].recordSync(command[frameIndex]);
            lightsGPU[frameIndex].recordSync(command[frameIndex]);
            GetAssetManager().recordSync(command[frameIndex]);

            // Skeleton mesh instance and bone matrix uploads
            skeletonMeshInstancesGPU[frameIndex].recordSyncRanges(command[frameIndex], dirtySkeletonMeshInstanceIndices[frameIndex]);
            skeletonInstancesGPU[frameIndex].recordSyncRanges(command[frameIndex], dirtySkeletonInstanceIndices_[frameIndex]);
            dirtySkeletonInstanceIndices_[frameIndex].clear();

            // Particle system instance uploads and deltaTime uniform
            particleSystemInstancesGPU[frameIndex].recordSyncRanges(command[frameIndex], dirtyParticleSystemIndices[frameIndex]);
            particleDeltaTimeBuffer[frameIndex].recordSync(command[frameIndex]);

            vkCmdFillBuffer(command[frameIndex].vkCommandBuffer, drawCounts[frameIndex].vkBuffer, 0, VK_WHOLE_SIZE, 0);
            for (auto& indirectBuffer : indirectBuffers[frameIndex]) {
                vkCmdFillBuffer(command[frameIndex].vkCommandBuffer, indirectBuffer.vkBuffer, 0, VK_WHOLE_SIZE, 0);
            }
            // Clear skeleton draw counts and indirect buffers
            vkCmdFillBuffer(command[frameIndex].vkCommandBuffer, skeletonDrawCounts[frameIndex].vkBuffer, 0, VK_WHOLE_SIZE, 0);
            for (auto& indirectBuffer : skeletonIndirectBuffers[frameIndex]) {
                vkCmdFillBuffer(command[frameIndex].vkCommandBuffer, indirectBuffer.vkBuffer, 0, VK_WHOLE_SIZE, 0);
            }
            // Clear depth pre-pass buffers — only zero the portion that the
            // uber cull shader can actually write to (objectInstances.getSize()
            // commands at most) plus the count word, instead of the full
            // MAX_OBJECTS * 20-byte buffer.
            {
                const VkDeviceSize usedBytes = static_cast<VkDeviceSize>(objectInstances.getSize()) * sizeof(VkDrawIndexedIndirectCommand);
                const VkDeviceSize fillSize  = usedBytes > 0 ? usedBytes : 4;
                vkCmdFillBuffer(command[frameIndex].vkCommandBuffer, depthPrePassBuffer[frameIndex].vkBuffer, 0, fillSize, 0);
            }
            vkCmdFillBuffer(command[frameIndex].vkCommandBuffer, depthPrePassCountBuffer[frameIndex].vkBuffer, 0, VK_WHOLE_SIZE, 0);
            // Clear skeleton depth pre-pass buffers
            {
                const VkDeviceSize usedBytes = static_cast<VkDeviceSize>(skeletonMeshInstances.getSize()) * sizeof(VkDrawIndexedIndirectCommand);
                const VkDeviceSize fillSize  = usedBytes > 0 ? usedBytes : 4;
                vkCmdFillBuffer(command[frameIndex].vkCommandBuffer, skeletonDepthPrePassBuffer[frameIndex].vkBuffer, 0, fillSize, 0);
            }
            vkCmdFillBuffer(command[frameIndex].vkCommandBuffer, skeletonDepthPrePassCountBuffer[frameIndex].vkBuffer, 0, VK_WHOLE_SIZE, 0);
            vkCmdFillBuffer(command[frameIndex].vkCommandBuffer, lightIndicesBuffer[frameIndex].vkBuffer, 0, VK_WHOLE_SIZE, 0);

            // Reset per-pipeline particle graphics indirect buffers and draw counts.
            // The cull shader packs visible draw commands from index 0 and writes the
            // per-pipeline count into particleGfxDrawCounts so the GPU knows how many to draw.
            if (!particleGfxIndirectBuffers.empty()) {
                vkCmdFillBuffer(command[frameIndex].vkCommandBuffer,
                    particleGfxDrawCounts[frameIndex].vkBuffer, 0, VK_WHOLE_SIZE, 0);
                for (auto& gfxBuf : particleGfxIndirectBuffers) {
                    vkCmdFillBuffer(command[frameIndex].vkCommandBuffer,
                        gfxBuf.vkBuffer, 0, VK_WHOLE_SIZE, 0);
                }
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

            // --- Skeleton cull compute dispatch ---
            {
                VkDescriptorSet descriptorSet = skeletonCullPipeline->getVkDescriptorSet(frameIndex);
                vkCmdBindPipeline(
                    command[frameIndex].vkCommandBuffer,
                    VK_PIPELINE_BIND_POINT_COMPUTE,
                    skeletonCullPipeline->getVkPipeline()
                );
                vkCmdBindDescriptorSets(
                    command[frameIndex].vkCommandBuffer,
                    VK_PIPELINE_BIND_POINT_COMPUTE,
                    skeletonCullPipeline->getVkPipelineLayout(),
                    0,
                    1,
                    &descriptorSet,
                    0,
                    nullptr
                );

                constexpr uint32_t WORKGROUP_SIZE_X = 256;
                if (skeletonMeshInstances.getSize() > 0) {
                    const uint32_t groupCountX = (skeletonMeshInstances.getSize() + WORKGROUP_SIZE_X - 1) / WORKGROUP_SIZE_X;
                    vkCmdDispatch(command[frameIndex].vkCommandBuffer, groupCountX, 1, 1);
                }
            }

            // --- Particle cull dispatch ---
            // One invocation per registered particle system slot.
            // Writes VkDrawIndexedIndirectCommand into particleGfxIndirectBuffer (per slot),
            // atomically accumulates x into particleCompIndirectBuffer (per comp pipeline),
            // and writes dispatchTable entries mapping global workgroup ID -> (particleBase, systemSlot).
            if (particleSystemRegisteredCount > 0 && particleCullPipeline) {
                // Reset x=0 for each active comp pipeline.  y and z remain 1 (written at registration).
                // vkCmdUpdateBuffer is a TRANSFER command, covered by the existing transferToShaderBarrier.
                for (uint32_t k = 0; k < static_cast<uint32_t>(particleCompPipelines.size()); ++k) {
                    const VkDispatchIndirectCommand reset{0, 1, 1};
                    vkCmdUpdateBuffer(command[frameIndex].vkCommandBuffer,
                        particleCompIndirectBuffer.vkBuffer,
                        k * sizeof(VkDispatchIndirectCommand),
                        sizeof(VkDispatchIndirectCommand), &reset);
                }

                struct ParticleCullPC { uint32_t systemCount; uint32_t _pad; };
                ParticleCullPC pc{ particleSystemRegisteredCount, 0u };
                VkDescriptorSet descSet = particleCullPipeline->getVkDescriptorSet(frameIndex);
                vkCmdBindPipeline(command[frameIndex].vkCommandBuffer,
                    VK_PIPELINE_BIND_POINT_COMPUTE,
                    particleCullPipeline->getVkPipeline());
                vkCmdBindDescriptorSets(command[frameIndex].vkCommandBuffer,
                    VK_PIPELINE_BIND_POINT_COMPUTE,
                    particleCullPipeline->getVkPipelineLayout(),
                    0, 1, &descSet, 0, nullptr);
                vkCmdPushConstants(command[frameIndex].vkCommandBuffer,
                    particleCullPipeline->getVkPipelineLayout(),
                    VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ParticleCullPC), &pc);

                constexpr uint32_t CULL_WORKGROUP = 64;
                const uint32_t cullGroups = (particleSystemRegisteredCount + CULL_WORKGROUP - 1) / CULL_WORKGROUP;
                vkCmdDispatch(command[frameIndex].vkCommandBuffer, cullGroups, 1, 1);

                // Barrier: cull writes (comp indirect x, gfx indirect, dispatch table, active flags)
                // must be visible before:
                //   - sim dispatches read comp indirect (INDIRECT_COMMAND_READ) and dispatch table (SHADER_READ)
                //   - draw-indirect stage reads gfx indirect (INDIRECT_COMMAND_READ, later in the frame)
                VkMemoryBarrier particleCullBarrier{};
                particleCullBarrier.sType         = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
                particleCullBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
                particleCullBarrier.dstAccessMask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT | VK_ACCESS_SHADER_READ_BIT;
                vkCmdPipelineBarrier(command[frameIndex].vkCommandBuffer,
                    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
                    0, 1, &particleCullBarrier, 0, nullptr, 0, nullptr);
            }

            // --- Particle simulation: ONE vkCmdDispatchIndirect per comp pipeline ---
            // The cull shader:
            //   - accumulated x = total workgroups into particleCompIndirectBuffer[K]
            //   - wrote dispatchTable[K * MAX_DISPATCH_PER_PIPELINE + (0..x-1)]
            //       = { particleBase, systemSlot } for every scheduled workgroup
            // The sim shader reads dispatchTable[pipelineIndex * MAX_DISPATCH_PER_PIPELINE
            //                                    + gl_WorkGroupID.x] to find its work.
            for (uint32_t compIdx = 0; compIdx < static_cast<uint32_t>(particleCompPipelines.size()); ++compIdx) {
                if (particleCompSystems[compIdx].empty()) continue;

                VkDescriptorSet descSet = particleCompPipelines[compIdx]->getVkDescriptorSet(frameIndex);
                vkCmdBindPipeline(command[frameIndex].vkCommandBuffer,
                    VK_PIPELINE_BIND_POINT_COMPUTE,
                    particleCompPipelines[compIdx]->getVkPipeline());
                vkCmdBindDescriptorSets(command[frameIndex].vkCommandBuffer,
                    VK_PIPELINE_BIND_POINT_COMPUTE,
                    particleCompPipelines[compIdx]->getVkPipelineLayout(),
                    0, 1, &descSet, 0, nullptr);
                // Pass pipelineIndex so the shader can compute its dispatch table base:
                //   dispatchTable[pipelineIndex * MAX_DISPATCH_PER_PIPELINE + gl_WorkGroupID.x]
                vkCmdPushConstants(command[frameIndex].vkCommandBuffer,
                    particleCompPipelines[compIdx]->getVkPipelineLayout(),
                    VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(uint32_t), &compIdx);
                // x = total workgroups for all visible systems using this pipeline.
                vkCmdDispatchIndirect(command[frameIndex].vkCommandBuffer,
                    particleCompIndirectBuffer.vkBuffer,
                    static_cast<VkDeviceSize>(compIdx) * sizeof(VkDispatchIndirectCommand));
            }

            {
                // Ensure compute writes (cull + particle simulation) are visible
                // to the subsequent depth pre-pass indirect draw and vertex reads.
                VkMemoryBarrier uberToDepthBarrier{};
                uberToDepthBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
                uberToDepthBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
                uberToDepthBarrier.dstAccessMask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT |
                                                   VK_ACCESS_SHADER_READ_BIT;

                vkCmdPipelineBarrier(
                    command[frameIndex].vkCommandBuffer,
                    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                    VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT |
                    VK_PIPELINE_STAGE_VERTEX_SHADER_BIT |
                    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                    0,
                    1,
                    &uberToDepthBarrier,
                    0,
                    nullptr,
                    0,
                    nullptr
                );
            }

            // --- Depth pre-pass: render into a single-sample depth map used by light culling ---
            {
                // Ensure depth map is transitioned to depth attachment optimal
                VkImageMemoryBarrier depthBarrierInit{};
                depthBarrierInit.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                depthBarrierInit.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
                depthBarrierInit.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
                depthBarrierInit.srcAccessMask = 0;
                depthBarrierInit.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
                depthBarrierInit.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                depthBarrierInit.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                depthBarrierInit.image = depthMaps[frameIndex].vkImage;
                depthBarrierInit.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
                depthBarrierInit.subresourceRange.baseMipLevel = 0;
                depthBarrierInit.subresourceRange.levelCount = 1;
                depthBarrierInit.subresourceRange.baseArrayLayer = 0;
                depthBarrierInit.subresourceRange.layerCount = 1;

                vkCmdPipelineBarrier(
                    command[frameIndex].vkCommandBuffer,
                    VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                    VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
                    0,
                    0, nullptr,
                    0, nullptr,
                    1, &depthBarrierInit
                );

                // Begin rendering to the depth map only
                VkRenderingAttachmentInfo depthAttachment{};
                depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
                depthAttachment.imageView = depthMaps[frameIndex].vkImageView;
                depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
                depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
                depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
                depthAttachment.clearValue.depthStencil = {1.0f, 0};

                VkRenderingInfo depthRenderingInfo{};
                depthRenderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
                depthRenderingInfo.renderArea.offset = {0, 0};
                depthRenderingInfo.renderArea.extent = vkExtent2D;
                depthRenderingInfo.layerCount = 1;
                depthRenderingInfo.viewMask = 0;
                depthRenderingInfo.colorAttachmentCount = 0;
                depthRenderingInfo.pColorAttachments = nullptr;
                depthRenderingInfo.pDepthAttachment = &depthAttachment;
                depthRenderingInfo.pStencilAttachment = nullptr;

                vkCmdBeginRendering(command[frameIndex].vkCommandBuffer, &depthRenderingInfo);

                // Bind depth pipeline and draw all meshes (indirect buffers per mesh)
                vkCmdBindPipeline(
                    command[frameIndex].vkCommandBuffer,
                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                    depthPipeline->getVkPipeline()
                );
                VkDescriptorSet depthDesc = depthPipeline->getVkDescriptorSet(frameIndex);
                vkCmdBindDescriptorSets(
                    command[frameIndex].vkCommandBuffer,
                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                    depthPipeline->getVkPipelineLayout(),
                    0,
                    1,
                    &depthDesc,
                    0,
                    nullptr
                );

                const Buffer vertexBuffer = GetAssetManager().getVertexBuffer();
                const Buffer indexBuffer = GetAssetManager().getIndexBuffer();
                VkBuffer vkVertexBuffer = vertexBuffer.vkBuffer;
                VkDeviceSize offsets[] = {0};
                vkCmdBindVertexBuffers(command[frameIndex].vkCommandBuffer, 0, 1, &vkVertexBuffer, offsets);
                vkCmdBindIndexBuffer(command[frameIndex].vkCommandBuffer, indexBuffer.vkBuffer, 0, VK_INDEX_TYPE_UINT32);

                const uint32_t meshPipelineCount = static_cast<uint32_t>(meshPipelines.size());
                LCA_ASSERT(meshPipelineCount == static_cast<uint32_t>(indirectBuffers[frameIndex].size()), "Renderer", "recordFrame", "meshPipelines and indirectBuffers size mismatch.");

                // Draw all visible objects using the single depth pre-pass indirect buffer
                vkCmdDrawIndexedIndirectCount(
                    command[frameIndex].vkCommandBuffer,
                    depthPrePassBuffer[frameIndex].vkBuffer,
                    0,
                    depthPrePassCountBuffer[frameIndex].vkBuffer,
                    0,
                    depthPrePassBuffer[frameIndex].numberElements,
                    sizeof(VkDrawIndexedIndirectCommand)
                );

                // --- Skeleton depth pre-pass ---
                vkCmdBindPipeline(
                    command[frameIndex].vkCommandBuffer,
                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                    skeletonDepthPipeline->getVkPipeline()
                );
                VkDescriptorSet skelDepthDesc = skeletonDepthPipeline->getVkDescriptorSet(frameIndex);
                vkCmdBindDescriptorSets(
                    command[frameIndex].vkCommandBuffer,
                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                    skeletonDepthPipeline->getVkPipelineLayout(),
                    0,
                    1,
                    &skelDepthDesc,
                    0,
                    nullptr
                );

                {
                    const Buffer skelVertexBuffer = GetAssetManager().getSkeletonVertexBuffer();
                    const Buffer skelIndexBuffer = GetAssetManager().getSkeletonIndexBuffer();
                    VkBuffer vkSkelVertexBuffer = skelVertexBuffer.vkBuffer;
                    vkCmdBindVertexBuffers(command[frameIndex].vkCommandBuffer, 0, 1, &vkSkelVertexBuffer, offsets);
                    vkCmdBindIndexBuffer(command[frameIndex].vkCommandBuffer, skelIndexBuffer.vkBuffer, 0, VK_INDEX_TYPE_UINT32);
                }

                vkCmdDrawIndexedIndirectCount(
                    command[frameIndex].vkCommandBuffer,
                    skeletonDepthPrePassBuffer[frameIndex].vkBuffer,
                    0,
                    skeletonDepthPrePassCountBuffer[frameIndex].vkBuffer,
                    0,
                    skeletonDepthPrePassBuffer[frameIndex].numberElements,
                    sizeof(VkDrawIndexedIndirectCommand)
                );

                vkCmdEndRendering(command[frameIndex].vkCommandBuffer);

                // Transition depth map to shader read for the light cull compute shader
                VkImageMemoryBarrier depthBarrierToRead{};
                depthBarrierToRead.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                depthBarrierToRead.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
                depthBarrierToRead.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
                depthBarrierToRead.oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
                depthBarrierToRead.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                depthBarrierToRead.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                depthBarrierToRead.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                depthBarrierToRead.image = depthMaps[frameIndex].vkImage;
                depthBarrierToRead.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
                depthBarrierToRead.subresourceRange.baseMipLevel = 0;
                depthBarrierToRead.subresourceRange.levelCount = 1;
                depthBarrierToRead.subresourceRange.baseArrayLayer = 0;
                depthBarrierToRead.subresourceRange.layerCount = 1;

                vkCmdPipelineBarrier(
                    command[frameIndex].vkCommandBuffer,
                    VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                    VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                    0,
                    0, nullptr,
                    0, nullptr,
                    1, &depthBarrierToRead
                );
            }

            // --- Light cull compute dispatch ---
            {
                
                VkDescriptorSet descriptorSet = lightCullPipeline->getVkDescriptorSet(frameIndex);
                vkCmdBindPipeline(
                    command[frameIndex].vkCommandBuffer,
                    VK_PIPELINE_BIND_POINT_COMPUTE,
                    lightCullPipeline->getVkPipeline()
                );
                vkCmdBindDescriptorSets(
                    command[frameIndex].vkCommandBuffer,
                    VK_PIPELINE_BIND_POINT_COMPUTE,
                    lightCullPipeline->getVkPipelineLayout(),
                    0,
                    1,
                    &descriptorSet,
                    0,
                    nullptr
                );

                const uint32_t groupCountX = (vkExtent2D.width + TILE_WIDTH - 1u) / TILE_WIDTH;
                const uint32_t groupCountY = (vkExtent2D.height + TILE_HEIGHT - 1u) / TILE_HEIGHT;
                vkCmdDispatch(command[frameIndex].vkCommandBuffer, groupCountX, groupCountY, 1);
            }

            // Make light cull compute writes visible to fragment shader SSBO reads.
            VkBufferMemoryBarrier lightIndicesBarrier{};
            lightIndicesBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
            lightIndicesBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            lightIndicesBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            lightIndicesBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            lightIndicesBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            lightIndicesBarrier.buffer = lightIndicesBuffer[frameIndex].vkBuffer;
            lightIndicesBarrier.offset = 0;
            lightIndicesBarrier.size = VK_WHOLE_SIZE;

            vkCmdPipelineBarrier(
                command[frameIndex].vkCommandBuffer,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                0,
                0,
                nullptr,
                1,
                &lightIndicesBarrier,
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

                // --- Skeleton mesh rendering ---
                {
                    const Buffer skelVertexBuffer = GetAssetManager().getSkeletonVertexBuffer();
                    const Buffer skelIndexBuffer = GetAssetManager().getSkeletonIndexBuffer();
                    VkBuffer vkSkelVertexBuffer = skelVertexBuffer.vkBuffer;
                    vkCmdBindVertexBuffers(command[frameIndex].vkCommandBuffer, 0, 1, &vkSkelVertexBuffer, offsets);
                    vkCmdBindIndexBuffer(command[frameIndex].vkCommandBuffer, skelIndexBuffer.vkBuffer, 0, VK_INDEX_TYPE_UINT32);

                    const uint32_t skelPipelineCount = static_cast<uint32_t>(skeletonMeshPipelines.size());
                    LCA_ASSERT(skelPipelineCount == static_cast<uint32_t>(skeletonIndirectBuffers[frameIndex].size()), "Renderer", "recordFrame", "skeletonMeshPipelines and skeletonIndirectBuffers size mismatch.");

                    for (uint32_t i = 0; i < skelPipelineCount; ++i) {
                        SkeletonMeshPipeline& skelPipeline = skeletonMeshPipelines[i];

                        VkDescriptorSet descriptorSet = skelPipeline.getVkDescriptorSet(frameIndex);
                        vkCmdBindPipeline(
                            command[frameIndex].vkCommandBuffer,
                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                            skelPipeline.getVkPipeline()
                        );
                        vkCmdBindDescriptorSets(
                            command[frameIndex].vkCommandBuffer,
                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                            skelPipeline.getVkPipelineLayout(),
                            0,
                            1,
                            &descriptorSet,
                            0,
                            nullptr
                        );

                        const Buffer& skelIndirectBuffer = skeletonIndirectBuffers[frameIndex][i];
                        const VkDeviceSize skelDrawCountOffset = static_cast<VkDeviceSize>(i) * sizeof(uint32_t);
                        vkCmdDrawIndexedIndirectCount(
                            command[frameIndex].vkCommandBuffer,
                            skelIndirectBuffer.vkBuffer,
                            0,
                            skeletonDrawCounts[frameIndex].vkBuffer,
                            skelDrawCountOffset,
                            skelIndirectBuffer.numberElements,
                            sizeof(VkDrawIndexedIndirectCommand)
                        );
                    }
                }

                // --- Particle system rendering ---
                // Each particle system has one VkDrawIndexedIndirectCommand at
                // particleGfxIndirectBuffer[slot].  The cull shader sets instanceCount=0
                // for culled systems so the GPU skips them with zero GPU work.
                if (!particlePipelines.empty()) {
                    const Buffer meshVertexBuffer = GetAssetManager().getVertexBuffer();
                    const Buffer meshIndexBuffer  = GetAssetManager().getIndexBuffer();
                    VkBuffer vkMeshVB = meshVertexBuffer.vkBuffer;
                    vkCmdBindVertexBuffers(command[frameIndex].vkCommandBuffer, 0, 1, &vkMeshVB, offsets);
                    vkCmdBindIndexBuffer(command[frameIndex].vkCommandBuffer, meshIndexBuffer.vkBuffer, 0, VK_INDEX_TYPE_UINT32);

                    for (uint32_t gfxIdx = 0; gfxIdx < static_cast<uint32_t>(particlePipelines.size()); ++gfxIdx) {
                        const auto& slots = particleGfxSystems[gfxIdx];
                        if (slots.empty()) continue;

                        VkDescriptorSet descSet = particlePipelines[gfxIdx].getVkDescriptorSet(frameIndex);
                        vkCmdBindPipeline(command[frameIndex].vkCommandBuffer,
                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                            particlePipelines[gfxIdx].getVkPipeline());
                        vkCmdBindDescriptorSets(command[frameIndex].vkCommandBuffer,
                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                            particlePipelines[gfxIdx].getVkPipelineLayout(),
                            0, 1, &descSet, 0, nullptr);

                        // One batched draw call per gfx pipeline.
                        // The cull shader packed visible draw commands from index 0 and wrote
                        // the count into particleGfxDrawCounts[gfxIdx].
                        //   firstInstance = particleOffset → gl_InstanceIndex = absolute particle index
                        // The vertex shader reads particle.params.y (floatBitsToUint) to get
                        // the system slot for transform and material lookup.
                        vkCmdDrawIndexedIndirectCount(
                            command[frameIndex].vkCommandBuffer,
                            particleGfxIndirectBuffers[gfxIdx].vkBuffer,
                            0,
                            particleGfxDrawCounts[frameIndex].vkBuffer,
                            static_cast<VkDeviceSize>(gfxIdx) * sizeof(uint32_t),
                            particleGfxIndirectBuffers[gfxIdx].numberElements,
                            sizeof(VkDrawIndexedIndirectCommand));
                    }
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

        // ── Skeleton mesh instances ────────────────────────────

        uint32_t Renderer::addSkeletonMeshInstance(const SkeletonMeshInstance& instance){
            return skeletonMeshInstances.add(instance);
        }

        void Renderer::updateSkeletonMeshInstance(uint32_t id, const SkeletonMeshInstance& instance){
            skeletonMeshInstances.update(id, instance);
        }

        void Renderer::removeSkeletonMeshInstance(uint32_t id){
            skeletonMeshInstances.remove(id);
        }

        // ── Skeleton instances (bone matrices) ─────────────────

        uint32_t Renderer::addSkeletonInstance(){
            uint32_t id;
            if(!freeSkeletonInstanceSlots.empty()){
                id = freeSkeletonInstanceSlots.back();
                freeSkeletonInstanceSlots.pop_back();
            } else {
                LCA_ASSERT(skeletonInstanceCount < MAX_SKELETON_INSTANCES, "Renderer", "addSkeletonInstance", "Exceeded MAX_SKELETON_INSTANCES capacity.")
                id = skeletonInstanceCount++;
            }
            return id;
        }

        void Renderer::updateSkeletonInstance(uint32_t frameIndex, uint32_t id, const SkeletonInstance& instance){
            LCA_ASSERT(id < MAX_SKELETON_INSTANCES, "Renderer", "updateSkeletonInstance", "Invalid skeleton instance ID.")
            // Protect writes to per-frame dirty index vector when system runs multi-threaded.
            memcpy(
                static_cast<char*>(skeletonInstancesGPU[frameIndex].interface.pMemory) + id * sizeof(SkeletonInstance),
                &instance,
                sizeof(SkeletonInstance)
            );
            std::lock_guard<std::mutex> lock(skeletonInstanceMutexes[frameIndex]);
            dirtySkeletonInstanceIndices_[frameIndex].push_back(id);
        }

        void Renderer::removeSkeletonInstance(uint32_t id){
            LCA_ASSERT(id < MAX_SKELETON_INSTANCES, "Renderer", "removeSkeletonInstance", "Invalid skeleton instance ID.")
            freeSkeletonInstanceSlots.push_back(id);
        }

        // ── Skeleton mesh pipelines ────────────────────────────

        uint32_t Renderer::addSkeletonMeshPipeline(const std::string& name, SkeletonMeshPipeline&& pipeline, uint32_t maxObjects){
            LCA_ASSERT(skeletonMeshPipelineMap.find(name) == skeletonMeshPipelineMap.end(), "Renderer", "addSkeletonMeshPipeline", "Skeleton mesh pipeline name already exists.")
            LCA_ASSERT(skeletonMeshPipelines.size() < MAX_SHADERS, "Renderer", "addSkeletonMeshPipeline", "Exceeded MAX_SHADERS capacity.")
            LCA_ASSERT(maxObjects > 0, "Renderer", "addSkeletonMeshPipeline", "maxObjects must be greater than 0.")

            skeletonMeshPipelines.push_back(std::move(pipeline));
            const uint32_t id = static_cast<uint32_t>(skeletonMeshPipelines.size() - 1);

            skeletonMeshPipelines.back().build();
            skeletonMeshPipelineMap[name] = id;

            for(uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
                Buffer buffer = createBuffer(maxObjects, sizeof(VkDrawIndexedIndirectCommand), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
                skeletonIndirectBuffers[i].push_back(buffer);
            }

            auto* capacities = static_cast<uint32_t*>(skeletonShaderCapacities.interface.pMemory);
            capacities[id] = maxObjects;

            beginSingleCommand(singleCommand);
            skeletonShaderCapacities.recordSync(singleCommand);
            endSingleCommand(singleCommand);
            submitSingleCommand(singleCommand);

            skeletonCullPipeline->updateDescriptorSetWrites();

            return id;
        }

        uint32_t Renderer::getSkeletonMeshPipelineId(const std::string& name) const {
            auto it = skeletonMeshPipelineMap.find(name);
            LCA_ASSERT(it != skeletonMeshPipelineMap.end(), "Renderer", "getSkeletonMeshPipelineId", "Skeleton mesh pipeline name not found.")
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

            // Build projection and view matrices. Include the X matrix used elsewhere
            // to match the existing viewProjection behavior.
            glm::mat4 proj = createProjectionMatrix(
                fov,
                static_cast<float>(vkExtent2D.width) / static_cast<float>(vkExtent2D.height),
                nearClip,
                farClip
            );
            glm::mat4 X = createXMatrix();
            glm::mat4 view = createViewMatrix(position, glm::normalize(direction), glm::vec3(0.f, -1.f, 0.f));

            cam.projection = proj * X;
            cam.inverseProjection = glm::inverse(cam.projection);
            cam.view = view;
            cam.viewProjection = cam.projection * cam.view;

            cam.camPos = glm::vec4(position, 0.0f);
            cam.camDir = glm::vec4(glm::normalize(direction), 0.0f);
            cam.screenSize = glm::vec2(static_cast<float>(vkExtent2D.width), static_cast<float>(vkExtent2D.height));
            cam.nearClip = nearClip;
            cam.farClip = farClip;

            // Extract and normalize frustum planes from viewProjection
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

            cam.frustumPlanes[0] = row3 + row0; // Left
            cam.frustumPlanes[1] = row3 - row0; // Right
            cam.frustumPlanes[2] = row3 + row1; // Bottom
            cam.frustumPlanes[3] = row3 - row1; // Top
            cam.frustumPlanes[4] = row2;        // Near
            cam.frustumPlanes[5] = row3 - row2; // Far

            for (glm::vec4& plane : cam.frustumPlanes) {
                const float normalLength = glm::length(glm::vec3(plane));
                if (normalLength > 0.0f) {
                    plane /= normalLength;
                }
            }

            memcpy(cameraBuffer[frameIndex].interface.pMemory, &cam, sizeof(Camera));
        }

        Texture& Renderer::getDepthMap(uint32_t frameIndex) {
            LCA_ASSERT(frameIndex < MAX_FRAMES_IN_FLIGHT, "Renderer", "getDepthMap", "Frame index out of range.")
            return depthMaps[frameIndex];
        }

        // ── Particle storage range allocation (mirrors AssetManager mesh strategy) ──

        uint32_t Renderer::allocateParticleRange(uint32_t count) {
            for (size_t i = 0; i < particleFreeRanges.size(); ++i) {
                if (particleFreeRanges[i].size >= count) {
                    const uint32_t offset = particleFreeRanges[i].offset;
                    if (particleFreeRanges[i].size == count) {
                        particleFreeRanges.erase(particleFreeRanges.begin() + static_cast<ptrdiff_t>(i));
                    } else {
                        particleFreeRanges[i].offset += count;
                        particleFreeRanges[i].size   -= count;
                    }
                    return offset;
                }
            }
            const uint32_t offset = currentParticleTop;
            currentParticleTop += count;
            return offset;
        }

        void Renderer::freeParticleRange(uint32_t offset, uint32_t count) {
            particleFreeRanges.push_back({offset, count});
        }

        // ── Particle system instance management ────────────────────────────────────

        uint32_t Renderer::addParticleSystem(const ParticleSystemInstance& instance) {
            LCA_ASSERT(instance.graphicsID < particlePipelines.size(), "Renderer", "addParticleSystem", "Invalid graphicsID.")
            LCA_ASSERT(instance.computeID  < particleCompPipelines.size(), "Renderer", "addParticleSystem", "Invalid computeID.")

            // --- Allocate particle range and fill in the offset ---
            const uint32_t particleOffset = allocateParticleRange(instance.particleCount);

            // --- Build the complete GPU instance record ---
            // indexCount / firstIndex / vertexOffset are stored in the instance so the cull
            // shader can write VkDrawIndexedIndirectCommand[slot] without a mesh-info SSBO.
            const auto meshInfo = GetAssetManager().getMeshInfo(instance.meshID);
            ParticleSystemInstance gpuInst  = instance;
            gpuInst.particleOffset          = particleOffset;
            gpuInst.active                  = 1u; // visible until cull decides otherwise
            gpuInst.indexCount              = meshInfo.indexCount;
            gpuInst.firstIndex              = meshInfo.firstIndex;
            gpuInst.vertexOffset            = meshInfo.vertexOffset;
            const uint32_t slot             = particleSystemSlots.add(gpuInst);

            // Track high-water for cull dispatch sizing
            particleSystemRegisteredCount = std::max(particleSystemRegisteredCount, slot + 1u);

            // --- Zero-initialise the particle storage range ---
            beginSingleCommand(singleCommand);
            vkCmdFillBuffer(singleCommand.vkCommandBuffer,
                particleStorageBuffer.vkBuffer,
                static_cast<VkDeviceSize>(particleOffset) * sizeof(Particle),
                static_cast<VkDeviceSize>(instance.particleCount) * sizeof(Particle),
                0u);
            endSingleCommand(singleCommand);
            submitSingleCommand(singleCommand);

            // Mark dirty in both frame slots so the DualBuffer upload happens next frame
            for (uint32_t fi = 0; fi < MAX_FRAMES_IN_FLIGHT; fi++) {
                dirtyParticleSystemIndices[fi].push_back(slot);
            }

            // --- Register for per-frame draw and sim tracking ---
            particleCompSystems[gpuInst.computeID].push_back(slot);
            particleGfxSystems[gpuInst.graphicsID].push_back(slot);

            return slot;
        }

        void Renderer::updateParticleSystem(uint32_t id, const ParticleSystemInstance& instance) {
            particleSystemSlots.update(id, instance);
        }

        void Renderer::removeParticleSystem(uint32_t id) {
            // Remove from per-pipeline registration lists
            for (auto& vec : particleCompSystems) {
                auto it = std::find(vec.begin(), vec.end(), id);
                if (it != vec.end()) { vec.erase(it); break; }
            }
            for (auto& vec : particleGfxSystems) {
                auto it = std::find(vec.begin(), vec.end(), id);
                if (it != vec.end()) { vec.erase(it); break; }
            }
            // No need to zero the gfx indirect slot: the cull shader will skip it once the slot
            // is marked inactive (transformID = UINT32_MAX after remove).
            particleSystemSlots.remove(id);
        }

        void Renderer::updateParticleDeltaTime(uint32_t frameIndex, float deltaTime) {
            ParticleDeltaTimeUniform tu{deltaTime, {0.0f, 0.0f, 0.0f}};
            memcpy(particleDeltaTimeBuffer[frameIndex].interface.pMemory, &tu, sizeof(ParticleDeltaTimeUniform));
        }

        // ── Particle compute pipeline management ───────────────────────────────────

        uint32_t Renderer::addParticleSystemCompPipeline(const std::string& name, ParticleSystemCompPipeline&& pipeline) {
            LCA_ASSERT(particleCompPipelineMap.find(name) == particleCompPipelineMap.end(),
                "Renderer", "addParticleSystemCompPipeline", "Particle comp pipeline name already exists.")
            LCA_ASSERT(particleCompPipelines.size() < Lca::Core::MAX_PARTICLE_COMP_PIPELINES,
                "Renderer", "addParticleSystemCompPipeline", "MAX_PARTICLE_COMP_PIPELINES exceeded.")

            const uint32_t id = static_cast<uint32_t>(particleCompPipelines.size());
            particleCompPipelines.push_back(std::make_unique<ParticleSystemCompPipeline>(std::move(pipeline)));
            particleCompPipelines.back()->build();
            particleCompPipelineMap[name] = id;
            particleCompSystems.emplace_back();

            // Write y=1, z=1 for this pipeline's indirect command slot.
            // x is reset to 0 each frame by vkCmdUpdateBuffer in recordFrame before the cull pass.
            // If the buffer is already initialised (init() has run), update it immediately.
            // If init() hasn't run yet, init() must write the y/z fields for all registered pipelines.
            if (particleCompIndirectBuffer.vkBuffer != VK_NULL_HANDLE) {
                beginSingleCommand(singleCommand);
                const VkDispatchIndirectCommand initCmd{0, 1, 1};
                vkCmdUpdateBuffer(singleCommand.vkCommandBuffer,
                    particleCompIndirectBuffer.vkBuffer,
                    id * sizeof(VkDispatchIndirectCommand),
                    sizeof(VkDispatchIndirectCommand), &initCmd);
                endSingleCommand(singleCommand);
                submitSingleCommand(singleCommand);
            }

            return id;
        }

        uint32_t Renderer::getParticleSystemCompId(const std::string& name) const {
            auto it = particleCompPipelineMap.find(name);
            LCA_ASSERT(it != particleCompPipelineMap.end(), "Renderer", "getParticleSystemCompId", "Particle comp pipeline not found.")
            return it->second;
        }

        // ── Particle graphics pipeline management ──────────────────────────────────

        uint32_t Renderer::addParticleSystemPipeline(const std::string& name, ParticleSystemPipeline&& pipeline) {
            LCA_ASSERT(particlePipelineMap.find(name) == particlePipelineMap.end(),
                "Renderer", "addParticleSystemPipeline", "Particle pipeline name already exists.")

            const uint32_t id = static_cast<uint32_t>(particlePipelines.size());
            particlePipelines.push_back(std::move(pipeline));
            particlePipelines.back().build();
            particlePipelineMap[name] = id;
            particleGfxSystems.emplace_back();
            // Create the per-pipeline graphics indirect buffer (one per gfx pipeline).
            // Indexed by global particle system slot so the cull shader can write
            //   firstInstance = particleOffset, instanceCount = particleCount
            // using only the global slot index — no pipelineLocalSlot remapping needed.
            Buffer gfxBuf = createBuffer(
                MAX_PARTICLE_SYSTEMS, sizeof(VkDrawIndexedIndirectCommand),
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT
                | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
            particleGfxIndirectBuffers.push_back(gfxBuf);
            // Refresh binding 3 of the cull pipeline descriptor so it sees the new buffer.
            if (particleCullPipeline) {
                particleCullPipeline->updateDescriptorSetWrites();
            }
            return id;
        }

        uint32_t Renderer::getParticleSystemPipelineId(const std::string& name) const {
            auto it = particlePipelineMap.find(name);
            LCA_ASSERT(it != particlePipelineMap.end(), "Renderer", "getParticleSystemPipelineId", "Particle pipeline not found.")
            return it->second;
        }
    }
}