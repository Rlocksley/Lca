#include "ParticleSystemCompPipeline.hpp"
#include "Renderer.hpp"

namespace Lca {
    namespace Core {

        ParticleSystemCompPipeline::ParticleSystemCompPipeline(const std::string& computeShaderPath)
            : ComputePipeline(computeShaderPath) {

            // Push constant: uint pipelineIndex (4 bytes).
            // Identifies this pipeline's strided segment in the dispatch table:
            //   dispatchTable[pipelineIndex * MAX_DISPATCH_PER_PIPELINE + gl_WorkGroupID.x]
            // The CPU passes compIdx at the vkCmdDispatchIndirect call site.
            addPushConstantRange(VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(uint32_t));

            for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
                // binding 0: particle system instances (particleOffset, particleCount, active, …)
                addStorageBuffer(i, 0, GetRenderer().getParticleSystemInstanceBuffer(i), VK_SHADER_STAGE_COMPUTE_BIT);
                // binding 1: particle storage (read current state, write updated state)
                addStorageBuffer(i, 1, GetRenderer().getParticleStorageBuffer(), VK_SHADER_STAGE_COMPUTE_BIT);
                // binding 2: model matrices (parent world transforms for world-space spawn)
                addStorageBuffer(i, 2, GetRenderer().getModelMatrixBuffer(i), VK_SHADER_STAGE_COMPUTE_BIT);
                // binding 3: deltaTime uniform (just the frame delta — lifetime is in the particle itself)
                addUniformBuffer(i, 3, GetRenderer().getParticleDeltaTimeBuffer(i), VK_SHADER_STAGE_COMPUTE_BIT);
                // binding 4: dispatch table — maps gl_WorkGroupID.x -> { particleBase, systemSlot }
                addStorageBuffer(i, 4, GetRenderer().getParticleDispatchTableBuffer(), VK_SHADER_STAGE_COMPUTE_BIT);
            }
        }

        void ParticleSystemCompPipeline::build() {
            ComputePipeline::build();
        }

    }
}
