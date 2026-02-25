#pragma once


#include "GraphicsPipeline.hpp"

namespace Lca {
    namespace Core{

        class DepthPipeline : public GraphicsPipeline{
            public:
                DepthPipeline(const GraphicsPipelineConfig& config);
                virtual ~DepthPipeline() override = default;
                
                virtual void build() override;

        };

    }
}