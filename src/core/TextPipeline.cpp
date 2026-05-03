#include "TextPipeline.hpp"
#include "AssetManager.hpp"
#include "Renderer.hpp"

namespace Lca {
    namespace Core {

        TextPipeline::TextPipeline(const GraphicsPipelineConfig& cfg)
            : GraphicsPipeline(cfg) {

            addVertex<Vertex::Mesh>();
            for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
                addUniformBuffer(i, 0, GetRenderer().getCameraBuffer(i), VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
                addStorageBuffer(i, 1, GetRenderer().getTextModelBuffer(i), VK_SHADER_STAGE_VERTEX_BIT);
                addStorageBuffer(i, 2, GetRenderer().getModelMatrixBuffer(i), VK_SHADER_STAGE_VERTEX_BIT);
                addStorageBuffer(i, 3, GetAssetManager().getMaterialBuffer(), VK_SHADER_STAGE_FRAGMENT_BIT);
                addTextureArray( i, 4, GetAssetManager().getTextures(), VK_SHADER_STAGE_FRAGMENT_BIT);
            }
        }

        void TextPipeline::build() {
            GraphicsPipeline::build();
        }

    }
}
