#include "MeshPipeline.hpp"
#include "AssetManager.hpp"
#include "Renderer.hpp"

namespace Lca {
    namespace Core {

        MeshPipeline::MeshPipeline(const GraphicsPipelineConfig& cfg)
            : GraphicsPipeline(cfg) {
            
            addVertex<Vertex::Mesh>();
            addUniformBuffer(0, GetRenderer().getCameraBuffer(), VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
            addStorageBuffer(1, GetRenderer().getObjectInstanceBuffer(), VK_SHADER_STAGE_VERTEX_BIT);
            addStorageBuffer(2, GetRenderer().getModelMatrixBuffer(), VK_SHADER_STAGE_VERTEX_BIT);
            addStorageBuffer(3, GetAssetManager().getMaterialBuffer(), VK_SHADER_STAGE_FRAGMENT_BIT);
            addTextureArray(4, GetAssetManager().getTextures(), VK_SHADER_STAGE_FRAGMENT_BIT);

        }

        void MeshPipeline::build() {            
            GraphicsPipeline::build();
        }

    }
}