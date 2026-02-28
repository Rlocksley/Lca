#pragma once

#include "ComputePipeline.hpp"

namespace Lca {
    namespace Core {

        class LightCullPipeline : public ComputePipeline {
        public:
            LightCullPipeline(const std::string& computeShaderPath);
            virtual ~LightCullPipeline() override = default;

            virtual void build() override;
        };

    }
}