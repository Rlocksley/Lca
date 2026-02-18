#include "GraphicsPipeline.hpp"

#include "Device.hpp"
#include "PhysicalDevice.hpp"
#include "RenderPass.hpp"
#include "Shader.hpp"

namespace Lca {
	namespace Core {

		GraphicsPipeline::GraphicsPipeline(const GraphicsPipelineConfig& cfg)
			: Pipeline(), config(cfg) {
			if (config.renderPass == VK_NULL_HANDLE) {
				config.renderPass = vkRenderPass;
			}
		}

		void GraphicsPipeline::build() {
			Pipeline::build();

			if (pipelineHandle() != VK_NULL_HANDLE) {
				vkDestroyPipeline(vkDevice, pipelineHandle(), nullptr);
				pipelineHandle() = VK_NULL_HANDLE;
			}

			LCA_ASSERT(!config.vertexShader.empty(), "GraphicsPipeline", "build", "Vertex shader path must be set.");
			LCA_ASSERT(!config.fragmentShader.empty(), "GraphicsPipeline", "build", "Fragment shader path must be set.");
			LCA_ASSERT(config.renderPass != VK_NULL_HANDLE, "GraphicsPipeline", "build", "Render pass must be valid.");

			Shader vertexShader(config.vertexShader);
			Shader fragmentShader(config.fragmentShader);

			VkShaderModule vertexModule = vertexShader.createShaderModule();
			VkShaderModule fragmentModule = fragmentShader.createShaderModule();

			VkPipelineShaderStageCreateInfo shaderStages[2]{};
			shaderStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
			shaderStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
			shaderStages[0].module = vertexModule;
			shaderStages[0].pName = "main";

			shaderStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
			shaderStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
			shaderStages[1].module = fragmentModule;
			shaderStages[1].pName = "main";

			VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo{};
			vertexInputStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

			const bool hasVertexBinding = (vertexInputBinding.stride > 0);
			vertexInputStateCreateInfo.vertexBindingDescriptionCount = hasVertexBinding ? 1u : 0u;
			vertexInputStateCreateInfo.pVertexBindingDescriptions = hasVertexBinding ? &vertexInputBinding : nullptr;
			vertexInputStateCreateInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexInputAttributes.size());
			vertexInputStateCreateInfo.pVertexAttributeDescriptions = vertexInputAttributes.empty() ? nullptr : vertexInputAttributes.data();

			VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCreateInfo{};
			inputAssemblyStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
			inputAssemblyStateCreateInfo.topology = config.topology;
			inputAssemblyStateCreateInfo.primitiveRestartEnable = VK_FALSE;

			VkViewport viewport{};
			viewport.x = 0.f;
			viewport.y = 0.f;
			viewport.width = static_cast<float>(vkExtent2D.width);
			viewport.height = static_cast<float>(vkExtent2D.height);
			viewport.minDepth = 0.f;
			viewport.maxDepth = 1.f;

			VkRect2D scissor{};
			scissor.offset = {0, 0};
			scissor.extent = vkExtent2D;

			VkPipelineViewportStateCreateInfo viewportStateCreateInfo{};
			viewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
			viewportStateCreateInfo.viewportCount = 1;
			viewportStateCreateInfo.pViewports = &viewport;
			viewportStateCreateInfo.scissorCount = 1;
			viewportStateCreateInfo.pScissors = &scissor;

			VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo{};
			rasterizationStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
			rasterizationStateCreateInfo.depthClampEnable = VK_FALSE;
			rasterizationStateCreateInfo.rasterizerDiscardEnable = VK_FALSE;
			rasterizationStateCreateInfo.polygonMode = config.polygonMode;
			rasterizationStateCreateInfo.cullMode = config.cullMode;
			rasterizationStateCreateInfo.frontFace = config.frontFace;
			rasterizationStateCreateInfo.lineWidth = 1.f;
			rasterizationStateCreateInfo.depthBiasEnable = VK_FALSE;

			VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfo{};
			multisampleStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
			multisampleStateCreateInfo.rasterizationSamples = vkSampleCountFlagBits;
			multisampleStateCreateInfo.sampleShadingEnable = VK_FALSE;

			VkPipelineDepthStencilStateCreateInfo depthStencilStateCreateInfo{};
			depthStencilStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
			depthStencilStateCreateInfo.depthTestEnable = config.depthTestEnable ? VK_TRUE : VK_FALSE;
			depthStencilStateCreateInfo.depthWriteEnable = config.depthWriteEnable ? VK_TRUE : VK_FALSE;
			depthStencilStateCreateInfo.depthCompareOp = config.depthCompareOp;
			depthStencilStateCreateInfo.depthBoundsTestEnable = VK_FALSE;
			depthStencilStateCreateInfo.stencilTestEnable = VK_FALSE;

			VkPipelineColorBlendAttachmentState colorBlendAttachmentState{};
			colorBlendAttachmentState.colorWriteMask =
				VK_COLOR_COMPONENT_R_BIT |
				VK_COLOR_COMPONENT_G_BIT |
				VK_COLOR_COMPONENT_B_BIT |
				VK_COLOR_COMPONENT_A_BIT;
			colorBlendAttachmentState.blendEnable = config.blendEnable ? VK_TRUE : VK_FALSE;
			colorBlendAttachmentState.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
			colorBlendAttachmentState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
			colorBlendAttachmentState.colorBlendOp = VK_BLEND_OP_ADD;
			colorBlendAttachmentState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
			colorBlendAttachmentState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
			colorBlendAttachmentState.alphaBlendOp = VK_BLEND_OP_ADD;

			VkPipelineColorBlendStateCreateInfo colorBlendStateCreateInfo{};
			colorBlendStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
			colorBlendStateCreateInfo.logicOpEnable = VK_FALSE;
			colorBlendStateCreateInfo.logicOp = VK_LOGIC_OP_COPY;
			colorBlendStateCreateInfo.attachmentCount = 1;
			colorBlendStateCreateInfo.pAttachments = &colorBlendAttachmentState;

			VkGraphicsPipelineCreateInfo graphicsPipelineCreateInfo{};
			graphicsPipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
			graphicsPipelineCreateInfo.stageCount = 2;
			graphicsPipelineCreateInfo.pStages = shaderStages;
			graphicsPipelineCreateInfo.pVertexInputState = &vertexInputStateCreateInfo;
			graphicsPipelineCreateInfo.pInputAssemblyState = &inputAssemblyStateCreateInfo;
			graphicsPipelineCreateInfo.pViewportState = &viewportStateCreateInfo;
			graphicsPipelineCreateInfo.pRasterizationState = &rasterizationStateCreateInfo;
			graphicsPipelineCreateInfo.pMultisampleState = &multisampleStateCreateInfo;
			graphicsPipelineCreateInfo.pDepthStencilState = &depthStencilStateCreateInfo;
			graphicsPipelineCreateInfo.pColorBlendState = &colorBlendStateCreateInfo;
			graphicsPipelineCreateInfo.layout = getVkPipelineLayout();
			graphicsPipelineCreateInfo.renderPass = config.renderPass;
			graphicsPipelineCreateInfo.subpass = config.subpass;
			graphicsPipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
			graphicsPipelineCreateInfo.basePipelineIndex = -1;

			LCA_CHECK_VULKAN(
				vkCreateGraphicsPipelines(vkDevice, VK_NULL_HANDLE, 1, &graphicsPipelineCreateInfo, nullptr, &pipelineHandle()),
				"GraphicsPipeline::build",
				"vkCreateGraphicsPipelines"
			)

			vkDestroyShaderModule(vkDevice, vertexModule, nullptr);
			vkDestroyShaderModule(vkDevice, fragmentModule, nullptr);
		}

	}
}
