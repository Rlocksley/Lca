#include "LightCullPipeline.hpp"
#include "Renderer.hpp"

namespace Lca {
    namespace Core {

        LightCullPipeline::LightCullPipeline(const std::string& computeShaderPath) : 
        ComputePipeline(computeShaderPath) 
        {
            for(uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
                addUniformBuffer(i, 0, GetRenderer().getCameraBuffer(i), VK_SHADER_STAGE_COMPUTE_BIT);
                addTexture(i, 1, GetRenderer().getDepthMap(i), VK_SHADER_STAGE_COMPUTE_BIT);
                addStorageBuffer(i, 2, GetRenderer().getLightBuffer(i), VK_SHADER_STAGE_COMPUTE_BIT);
                addStorageBuffer(i, 3, GetRenderer().getLightIndicesBuffer(i), VK_SHADER_STAGE_COMPUTE_BIT);   
            }
        }


        void LightCullPipeline::build() {
            ComputePipeline::build();
        }

    }
}