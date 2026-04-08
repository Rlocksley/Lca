#pragma once

#include "GraphicsPipeline.hpp"

namespace Lca {
    namespace Core {

        // Graphics pipeline for rendering particle systems.
        // Each registered particle system is drawn as part of a single batched
        // vkCmdDrawIndexedIndirect call covering all registered system slots.
        //   instanceCount = particleCount  (0 for culled/freed slots)
        //   firstInstance = particleOffset
        // The vertex shader uses gl_InstanceIndex (= firstInstance + local_index)
        // as the absolute index into the particle storage buffer, then reads
        // particle.params.y (floatBitsToUint) to look up the ParticleSystemInstance
        // for transform, localOffset, and material data.
        class ParticleSystemPipeline : public GraphicsPipeline {
        public:
            ParticleSystemPipeline(const GraphicsPipelineConfig& config);
            virtual ~ParticleSystemPipeline() override = default;

            virtual void build() override;
        };

    }
}
