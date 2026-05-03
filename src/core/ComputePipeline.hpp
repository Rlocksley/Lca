#pragma once

#include "Pipeline.hpp"

namespace Lca {
    namespace Core {

        class ComputePipeline : public Pipeline {
        public:
            ComputePipeline(const std::string& computeShaderPath) : Pipeline(), computeShader(computeShaderPath) {
            }
            ComputePipeline(ComputePipeline&&) noexcept = default;
            virtual ~ComputePipeline() override = default;

            virtual void build() override;

        private:
            const std::string computeShader;

        };

    }
}