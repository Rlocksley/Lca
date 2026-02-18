#pragma once

#include "Pipeline.hpp"

namespace Lca {
    namespace Core {

        class ComputePipeline : public Pipeline {
        public:
            ComputePipeline(const std::string& computeShaderPath) : Pipeline(), computeShader(computeShaderPath) {
            }
            virtual ~ComputePipeline() override = default;

            virtual void build() override;

        private:
            const std::string computeShader;

        };

    }
}