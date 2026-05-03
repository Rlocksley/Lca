#pragma once

#include "GraphicsPipeline.hpp"

namespace Lca {
    namespace Core{

        class SkeletonMeshPipeline : public GraphicsPipeline{
            public:
                SkeletonMeshPipeline(const GraphicsPipelineConfig& config);
                SkeletonMeshPipeline(SkeletonMeshPipeline&&) noexcept = default;
                SkeletonMeshPipeline& operator=(SkeletonMeshPipeline&&) noexcept = default;
                virtual ~SkeletonMeshPipeline() override = default;
                
                virtual void build() override;

        };

    }
}
