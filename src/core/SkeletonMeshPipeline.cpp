#include "SkeletonMeshPipeline.hpp"
#include "AssetManager.hpp"
#include "Renderer.hpp"

namespace Lca {
    namespace Core {

        SkeletonMeshPipeline::SkeletonMeshPipeline(const GraphicsPipelineConfig& cfg)
            : GraphicsPipeline(cfg) {
            
            addVertex<Vertex::Skeleton>();
            for(uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
                addUniformBuffer(i, 0, GetRenderer().getCameraBuffer(i), VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
                addStorageBuffer(i, 1, GetRenderer().getSkeletonMeshInstanceBuffer(i), VK_SHADER_STAGE_VERTEX_BIT);
                addStorageBuffer(i, 2, GetRenderer().getModelMatrixBuffer(i), VK_SHADER_STAGE_VERTEX_BIT);
                addStorageBuffer(i, 3, GetAssetManager().getMaterialBuffer(), VK_SHADER_STAGE_FRAGMENT_BIT);
                addTextureArray( i, 4, GetAssetManager().getTextures(), VK_SHADER_STAGE_FRAGMENT_BIT);
                addStorageBuffer(i, 5, GetRenderer().getLightBuffer(i), VK_SHADER_STAGE_FRAGMENT_BIT);
                addStorageBuffer(i, 6, GetRenderer().getLightIndicesBuffer(i), VK_SHADER_STAGE_FRAGMENT_BIT);
                addStorageBuffer(i, 7, GetRenderer().getSkeletonInstanceBuffer(i), VK_SHADER_STAGE_VERTEX_BIT);
            }
        }

        void SkeletonMeshPipeline::build() {            
            GraphicsPipeline::build();
        }

    }
}
