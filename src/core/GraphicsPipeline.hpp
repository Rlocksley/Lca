#pragma once

#include "Global.hpp"
#include "Pipeline.hpp"

namespace Lca{
    namespace Core{

        struct GraphicsPipelineConfig{
            std::string vertexShader;
            std::string fragmentShader;
            VkRenderPass renderPass{VK_NULL_HANDLE};
            uint32_t subpass{0};

            VkPrimitiveTopology topology{VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST};
            VkPolygonMode polygonMode{VK_POLYGON_MODE_FILL};
            VkCullModeFlags cullMode{VK_CULL_MODE_BACK_BIT};
            VkFrontFace frontFace{VK_FRONT_FACE_CLOCKWISE};

            bool depthTestEnable{true};
            bool depthWriteEnable{true};
            VkCompareOp depthCompareOp{VK_COMPARE_OP_LESS};

            bool blendEnable{true};
        };

        class GraphicsPipeline : public Pipeline{

            public:
                GraphicsPipeline(const GraphicsPipelineConfig& config);
                virtual ~GraphicsPipeline() override = default;
                
                virtual void build() override;

            template<typename T>
            GraphicsPipeline& addVertex(){
                vertexInputBinding = T::getBindingDescription();
                vertexInputAttributes = T::getAttributeDescriptions();
                return *this;
            }

            private:
                GraphicsPipelineConfig config;
                
                VkVertexInputBindingDescription vertexInputBinding{};
                std::vector<VkVertexInputAttributeDescription> vertexInputAttributes;
            
        };

    }
}
