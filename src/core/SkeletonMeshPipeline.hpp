#pragma once

#include "GraphicsPipeline.hpp"

namespace Lca {
    namespace Core{

        class SkeletonMeshPipeline : public GraphicsPipeline{
            public:
                SkeletonMeshPipeline(const GraphicsPipelineConfig& config);
                virtual ~SkeletonMeshPipeline() override = default;
                
                virtual void build() override;

        };

    }
}
