#include "ParticleCullPipeline.hpp"
#include "Renderer.hpp"

namespace Lca {
    namespace Core {

        ParticleCullPipeline::ParticleCullPipeline(const std::string& computeShaderPath)
            : ComputePipeline(computeShaderPath) {

            // Push constant: uint systemCount, uint _pad (8 bytes)
            addPushConstantRange(VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(uint32_t) * 2);

            for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
                // binding 0: camera (frustum planes)
                addUniformBuffer(i, 0, GetRenderer().getCameraBuffer(i), VK_SHADER_STAGE_COMPUTE_BIT);
                // binding 1: particle system instances (read props, write active flag)
                addStorageBuffer(i, 1, GetRenderer().getParticleSystemInstanceBuffer(i), VK_SHADER_STAGE_COMPUTE_BIT);
                // binding 2: model matrices (to compute bounding sphere world centre)
                addStorageBuffer(i, 2, GetRenderer().getModelMatrixBuffer(i), VK_SHADER_STAGE_COMPUTE_BIT);
                // binding 3: array of per-pipeline graphics indirect buffers
                //   [graphicsID][slot] = VkDrawIndexedIndirectCommand
                //   firstInstance = particleOffset, instanceCount = particleCount (0 = culled)
                //   Indexed by global particle system slot; buffers pre-zeroed each frame.
                addBufferArray(i, 3, GetRenderer().getParticleGfxIndirectBuffers(),
                    MAX_PARTICLE_GFX_PIPELINES, GetRenderer().getDummyIndirectVkBuffer(),
                    VK_SHADER_STAGE_COMPUTE_BIT);
                // binding 4: comp indirect buffer [MAX_PARTICLE_COMP_PIPELINES]
                //   cull atomically adds numGroups to VkDispatchIndirectCommand[computeID].x
                //   using atomicAdd; y and z are always 1 (written at pipeline registration)
                addStorageBuffer(i, 4, GetRenderer().getParticleCompIndirectBuffer(), VK_SHADER_STAGE_COMPUTE_BIT);
                // binding 5: dispatch table [MAX_PARTICLE_DISPATCH_ENTRIES]
                //   cull writes { particleBase, systemSlot } for each workgroup it schedules;
                //   sim shader reads dispatchTable[gl_WorkGroupID.x] to find its system
                addStorageBuffer(i, 5, GetRenderer().getParticleDispatchTableBuffer(), VK_SHADER_STAGE_COMPUTE_BIT);
                // binding 6: per-pipeline gfx draw counts [MAX_PARTICLE_GFX_PIPELINES]
                //   cull atomically increments counts[graphicsID] for each visible system;
                //   draw-indirect stage uses vkCmdDrawIndexedIndirectCount to read the packed count
                addStorageBuffer(i, 6, GetRenderer().getParticleGfxDrawCountBuffer(i), VK_SHADER_STAGE_COMPUTE_BIT);
            }
        }

        void ParticleCullPipeline::build() {
            ComputePipeline::build();
        }

    }
}
