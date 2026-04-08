#include "ParticleSystemPipeline.hpp"
#include "AssetManager.hpp"
#include "Renderer.hpp"

namespace Lca {
    namespace Core {

        ParticleSystemPipeline::ParticleSystemPipeline(const GraphicsPipelineConfig& cfg)
            : GraphicsPipeline(cfg) {

            addVertex<Vertex::Mesh>();
            for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
                // binding 0: camera uniform
                addUniformBuffer(i, 0, GetRenderer().getCameraBuffer(i), VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
                // binding 1: particle system instances (for transform, material, localOffset)
                addStorageBuffer(i, 1, GetRenderer().getParticleSystemInstanceBuffer(i), VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
                // binding 2: model matrices (parent world transforms)
                addStorageBuffer(i, 2, GetRenderer().getModelMatrixBuffer(i), VK_SHADER_STAGE_VERTEX_BIT);
                // binding 3: material buffer
                addStorageBuffer(i, 3, GetAssetManager().getMaterialBuffer(), VK_SHADER_STAGE_FRAGMENT_BIT);
                // binding 4: texture array
                addTextureArray(i, 4, GetAssetManager().getTextures(), VK_SHADER_STAGE_FRAGMENT_BIT);
                // binding 5: light buffer
                addStorageBuffer(i, 5, GetRenderer().getLightBuffer(i), VK_SHADER_STAGE_FRAGMENT_BIT);
                // binding 6: tile light indices
                addStorageBuffer(i, 6, GetRenderer().getLightIndicesBuffer(i), VK_SHADER_STAGE_FRAGMENT_BIT);
                // binding 7: particle storage (indexed by gl_InstanceIndex = absolute particle index)
                addStorageBuffer(i, 7, GetRenderer().getParticleStorageBuffer(), VK_SHADER_STAGE_VERTEX_BIT);
            }
        }

        void ParticleSystemPipeline::build() {
            GraphicsPipeline::build();
        }

    }
}
