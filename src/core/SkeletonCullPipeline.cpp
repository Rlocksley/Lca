#include "SkeletonCullPipeline.hpp"

#include "AssetManager.hpp"
#include "Renderer.hpp"

namespace Lca {
	namespace Core {

		SkeletonCullPipeline::SkeletonCullPipeline(const std::string& computeShaderPath)
			: ComputePipeline(computeShaderPath) {

			for(uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
				addUniformBuffer(i, 0, GetRenderer().getCameraBuffer(i), VK_SHADER_STAGE_COMPUTE_BIT);
				addStorageBuffer(i, 1, GetRenderer().getSkeletonMeshInstanceBuffer(i), VK_SHADER_STAGE_COMPUTE_BIT);
				addStorageBuffer(i, 2, GetRenderer().getModelMatrixBuffer(i), VK_SHADER_STAGE_COMPUTE_BIT);
				addStorageBuffer(i, 3, GetAssetManager().getSkeletonMeshInfoBuffer(), VK_SHADER_STAGE_COMPUTE_BIT);
				addStorageBuffer(i, 4, GetRenderer().getSkeletonDrawCountBuffer(i), VK_SHADER_STAGE_COMPUTE_BIT);
				addBufferArray(
					i,
					5,
					GetRenderer().getSkeletonIndirectBuffers(i),
					GetRenderer().getMaxShaderCount(),
					GetRenderer().getSkeletonDummyIndirectVkBuffer(),
					VK_SHADER_STAGE_COMPUTE_BIT
				);
				addStorageBuffer(i, 6, GetRenderer().getSkeletonShaderCapacityBuffer(), VK_SHADER_STAGE_COMPUTE_BIT);
				addStorageBuffer(i, 7, GetRenderer().skeletonDepthPrePassBuffer[i], VK_SHADER_STAGE_COMPUTE_BIT);
				addStorageBuffer(i, 8, GetRenderer().skeletonDepthPrePassCountBuffer[i], VK_SHADER_STAGE_COMPUTE_BIT);
				addStorageBuffer(i, 9, GetRenderer().getSkeletonInstanceBuffer(i), VK_SHADER_STAGE_COMPUTE_BIT);
			}
		}

		void SkeletonCullPipeline::build() {
			ComputePipeline::build();
		}

	}
}
