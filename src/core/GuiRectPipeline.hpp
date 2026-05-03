#pragma once

#include "GraphicsPipeline.hpp"

namespace Lca {
    namespace Core {

        class GuiRectPipeline : public GraphicsPipeline {
        public:
            GuiRectPipeline(const GraphicsPipelineConfig& config);
            GuiRectPipeline(GuiRectPipeline&&) noexcept = default;
            GuiRectPipeline& operator=(GuiRectPipeline&&) noexcept = default;
            virtual ~GuiRectPipeline() override = default;

            virtual void build() override;
        };

    }
}
