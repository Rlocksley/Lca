#pragma once

#include "ComputePipeline.hpp"

namespace Lca {
    namespace Core {

        class SkeletonCullPipeline : public ComputePipeline {
        public:
            SkeletonCullPipeline(const std::string& computeShaderPath);
            virtual ~SkeletonCullPipeline() override = default;

            virtual void build() override;

        };

    }
}
