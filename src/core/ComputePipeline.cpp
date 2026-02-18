#include "ComputePipeline.hpp"

#include "Device.hpp"
#include "Shader.hpp"

namespace Lca {
	namespace Core {

		void ComputePipeline::build() {
			Pipeline::build();

			if (pipelineHandle() != VK_NULL_HANDLE) {
				vkDestroyPipeline(vkDevice, pipelineHandle(), nullptr);
				pipelineHandle() = VK_NULL_HANDLE;
			}

			LCA_ASSERT(!computeShader.empty(), "ComputePipeline", "build", "Compute shader path must be set.")

			Shader shader(computeShader);
			VkShaderModule shaderModule = shader.createShaderModule();

			VkPipelineShaderStageCreateInfo shaderStage{};
			shaderStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
			shaderStage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
			shaderStage.module = shaderModule;
			shaderStage.pName = "main";

			VkComputePipelineCreateInfo computePipelineCreateInfo{};
			computePipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
			computePipelineCreateInfo.stage = shaderStage;
			computePipelineCreateInfo.layout = getVkPipelineLayout();
			computePipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
			computePipelineCreateInfo.basePipelineIndex = -1;

			LCA_CHECK_VULKAN(
				vkCreateComputePipelines(vkDevice, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &pipelineHandle()),
				"ComputePipeline::build",
				"vkCreateComputePipelines"
			)

			vkDestroyShaderModule(vkDevice, shaderModule, nullptr);
		}

	}
}
