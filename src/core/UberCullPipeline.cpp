#include "UberCullPipeline.hpp"

#include "AssetManager.hpp"
#include "Renderer.hpp"

namespace Lca {
	namespace Core {

		UberCullPipeline::UberCullPipeline(const std::string& computeShaderPath)
			: ComputePipeline(computeShaderPath) {

			for(uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
				addUniformBuffer(i, 0, GetRenderer().getCameraBuffer(i), VK_SHADER_STAGE_COMPUTE_BIT);
				addStorageBuffer(i, 1, GetRenderer().getObjectInstanceBuffer(i), VK_SHADER_STAGE_COMPUTE_BIT);
				addStorageBuffer(i, 2, GetRenderer().getModelMatrixBuffer(i), VK_SHADER_STAGE_COMPUTE_BIT);
				addStorageBuffer(i, 3, GetAssetManager().getMeshInfoBuffer(), VK_SHADER_STAGE_COMPUTE_BIT);
				addStorageBuffer(i, 4, GetRenderer().getDrawCountBuffer(i), VK_SHADER_STAGE_COMPUTE_BIT);
				addBufferArray(
					i,
					5,
					GetRenderer().getIndirectBuffers(i),
					GetRenderer().getMaxShaderCount(),
					GetRenderer().getDummyIndirectVkBuffer(),
					VK_SHADER_STAGE_COMPUTE_BIT
				);
				addStorageBuffer(i, 6, GetRenderer().getShaderCapacityBuffer(), VK_SHADER_STAGE_COMPUTE_BIT);
			}
		}

		void UberCullPipeline::build() {
			ComputePipeline::build();
		}

	}
}
