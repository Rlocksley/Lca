#include "SkeletonDepthPipeline.hpp"
#include "AssetManager.hpp"
#include "Renderer.hpp"

namespace Lca {
    namespace Core {

        SkeletonDepthPipeline::SkeletonDepthPipeline(const GraphicsPipelineConfig& cfg)
            : GraphicsPipeline(cfg) {
            
            addDepthVertex<Vertex::Skeleton>();
            for(uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
                addUniformBuffer(i, 0, GetRenderer().getCameraBuffer(i), VK_SHADER_STAGE_VERTEX_BIT);
                addStorageBuffer(i, 1, GetRenderer().getSkeletonMeshInstanceBuffer(i), VK_SHADER_STAGE_VERTEX_BIT);
                addStorageBuffer(i, 2, GetRenderer().getModelMatrixBuffer(i), VK_SHADER_STAGE_VERTEX_BIT);
                addStorageBuffer(i, 3, GetRenderer().getSkeletonInstanceBuffer(i), VK_SHADER_STAGE_VERTEX_BIT);
            }
        }

        void SkeletonDepthPipeline::build() {            
            GraphicsPipeline::build();
        }

    }
}
