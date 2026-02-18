#include "UberCullPipeline.hpp"

#include "AssetManager.hpp"
#include "Renderer.hpp"

namespace Lca {
	namespace Core {

		UberCullPipeline::UberCullPipeline(const std::string& computeShaderPath)
			: ComputePipeline(computeShaderPath) {

			addUniformBuffer(0, GetRenderer().getCameraBuffer(), VK_SHADER_STAGE_COMPUTE_BIT);
			addStorageBuffer(1, GetRenderer().getObjectInstanceBuffer(), VK_SHADER_STAGE_COMPUTE_BIT);
			addStorageBuffer(2, GetRenderer().getModelMatrixBuffer(), VK_SHADER_STAGE_COMPUTE_BIT);
			addStorageBuffer(3, GetAssetManager().getMeshInfoBuffer(), VK_SHADER_STAGE_COMPUTE_BIT);
			addStorageBuffer(4, GetRenderer().getDrawCountBuffer(), VK_SHADER_STAGE_COMPUTE_BIT);
			addBufferArray(
				5,
				GetRenderer().getIndirectBuffers(),
				GetRenderer().getMaxShaderCount(),
				GetRenderer().getDummyIndirectVkBuffer(),
				VK_SHADER_STAGE_COMPUTE_BIT
			);
			addStorageBuffer(6, GetRenderer().getShaderCapacityBuffer(), VK_SHADER_STAGE_COMPUTE_BIT);
		}

		void UberCullPipeline::build() {
			ComputePipeline::build();
		}

	}
}
