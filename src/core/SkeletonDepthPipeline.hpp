#pragma once

#include "GraphicsPipeline.hpp"

namespace Lca {
    namespace Core{

        class SkeletonDepthPipeline : public GraphicsPipeline{
            public:
                SkeletonDepthPipeline(const GraphicsPipelineConfig& config);
                virtual ~SkeletonDepthPipeline() override = default;
                
                virtual void build() override;

        };

    }
}
