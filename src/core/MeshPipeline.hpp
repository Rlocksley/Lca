#pragma once


#include "GraphicsPipeline.hpp"

namespace Lca {
    namespace Core{

        class MeshPipeline : public GraphicsPipeline{
            public:
                MeshPipeline(const GraphicsPipelineConfig& config);
                virtual ~MeshPipeline() override = default;
                
                virtual void build() override;

        };

    }
}