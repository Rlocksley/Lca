#pragma once

#include "ComputePipeline.hpp"

namespace Lca {
    namespace Core {

        class UberCullPipeline : public ComputePipeline {
        public:
            UberCullPipeline(const std::string& computeShaderPath);
            virtual ~UberCullPipeline() override = default;

            virtual void build() override;

        };

    }
}