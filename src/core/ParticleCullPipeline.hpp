#pragma once

#include "ComputePipeline.hpp"

namespace Lca {
    namespace Core {

        // Compute pipeline that frustum-culls all registered particle systems
        // using per-system bounding spheres and writes GPU-driven indirect commands.
        //
        // One dispatch per frame: ceil(particleSystemRegisteredCount / 64) workgroups.
        // Each invocation handles one particle system slot:
        //   – Bounding sphere (centre = modelMatrix[transformID] * localOffset * vec4(0,0,0,1),
        //     radius = ParticleSystemInstance.boundingSphereRadius) vs camera frustum planes.
        //   – If visible:
        //       particleGfxIndirectBuffers[graphicsID][slot]    = full VkDrawIndexedIndirectCommand
        //         (firstInstance = particleOffset, instanceCount = particleCount)
        //       particleCompIndirectBuffer[computeID].x    += ceil(particleCount/256) (atomicAdd)
        //       dispatchTable[baseEntry..baseEntry+N-1]     = { particleBase, systemSlot }
        //         (N = ceil(particleCount/256); one entry per workgroup of the sim shader)
        //   – If culled:
        //       particleGfxIndirectBuffers[graphicsID][slot] = already 0 (pre-zeroed by fill)
        //       (nothing added to comp indirect — x stays 0 for fully-culled pipelines)
        //
        // Push constant layout (8 bytes):
        //   uint systemCount  – how many slots to test (particleSystemRegisteredCount)
        //   uint _pad         – reserved
        //
        // Bindings:
        //   0 – Camera uniform (frustum planes)
        //   1 – ParticleSystemInstances[] (read props; write active)
        //   2 – ModelMatrices[] (bounding sphere world centre)
        //   3 – particleGfxIndirectBuffers[] (array of per-pipeline SSBOs, indexed by graphicsID)
        //         writes packed VkDrawIndexedIndirectCommand[cmdIdx] with firstInstance=particleOffset
        //   4 – particleCompIndirectBuffer (atomicAdd workgroup count per comp pipeline)
        //   5 – particleDispatchTableBuffer (write { particleBase, systemSlot } per workgroup)
        //   6 – particleGfxDrawCounts (atomicAdd visible draw count per gfx pipeline)
        class ParticleCullPipeline : public ComputePipeline {
        public:
            explicit ParticleCullPipeline(const std::string& computeShaderPath);
            virtual ~ParticleCullPipeline() override = default;

            virtual void build() override;
        };

    }
}
