#pragma once

#include "ComputePipeline.hpp"

namespace Lca {
    namespace Core {

        // Compute pipeline for simulating particle movement.
        // ONE vkCmdDispatchIndirect per pipeline kind per frame (not per system).
        // Push constant `uint pipelineIndex` (= K) lets the shader index its
        // strided region in the dispatch table:
        //   dispatchTable[K * MAX_DISPATCH_PER_PIPELINE + gl_WorkGroupID.x]
        //     → { particleBase, systemSlot }
        // The total workgroup count comes from particleCompIndirectBuffer[K].x,
        // written by the ParticleCullPipeline.
        class ParticleSystemCompPipeline : public ComputePipeline {
        public:
            ParticleSystemCompPipeline(const std::string& computeShaderPath);
            virtual ~ParticleSystemCompPipeline() override = default;

            virtual void build() override;
        };

    }
}
