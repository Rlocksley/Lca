#pragma once

#include "Global.hpp"
#include "Pipeline.hpp"

namespace Lca{
    namespace Core{

        struct GraphicsPipelineConfig{
            std::string vertexShader;
            std::string fragmentShader;
        
            VkExtent2D extend2D{0,0};

            VkPrimitiveTopology topology{VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST};
            VkPolygonMode polygonMode{VK_POLYGON_MODE_FILL};
            VkCullModeFlags cullMode{VK_CULL_MODE_BACK_BIT};
            VkFrontFace frontFace{VK_FRONT_FACE_CLOCKWISE};
            float depthBiasConstantFactor{0.0f};
            float depthBiasClamp{0.0f};
            float depthBiasSlopeFactor{0.0f};

            VkSampleCountFlagBits sampleCount{VK_SAMPLE_COUNT_FLAG_BITS_MAX_ENUM};
            float minSampleShading{0.0f};

            bool depthTestEnable{true};
            bool depthWriteEnable{true};
            VkCompareOp depthCompareOp{VK_COMPARE_OP_LESS};
            float minDepthBounds{0.0f};
            float maxDepthBounds{0.0f};

            bool hasColorAttachments{true};

            bool blendEnable{true};

            std::vector<VkFormat> colorAttachments{};
            VkFormat depthAttachmentFormat{VK_FORMAT_D32_SFLOAT};
            uint32_t viewMask{0};
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

            template<typename T>
            GraphicsPipeline& addDepthVertex(){
                vertexInputBinding = T::getDepthBindingDescription();
                vertexInputAttributes = T::getDepthAttributeDescriptions();
                return *this;
            }


            private:
                GraphicsPipelineConfig config;
                
                VkVertexInputBindingDescription vertexInputBinding{};
                std::vector<VkVertexInputAttributeDescription> vertexInputAttributes;
            
        };

    }
}
