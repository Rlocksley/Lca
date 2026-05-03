#pragma once

#include "GraphicsPipeline.hpp"

namespace Lca {
    namespace Core {

        class TextPipeline : public GraphicsPipeline {
        public:
            TextPipeline(const GraphicsPipelineConfig& config);
            TextPipeline(TextPipeline&&) noexcept = default;
            TextPipeline& operator=(TextPipeline&&) noexcept = default;
            virtual ~TextPipeline() override = default;

            virtual void build() override;
        };

    }
}
