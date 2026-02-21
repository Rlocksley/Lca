#pragma once

#include "Global.hpp"
#include "Image.hpp"

namespace Lca
{
    namespace Core
    {
        struct Framebuffers
        {
            std::vector<Image> colorImages;
            std::vector<Image> depthImages;
            std::vector<VkFramebuffer> vkFramebuffers{};
        };

        inline Framebuffers framebuffers;

        void createFramebuffers();
        void destroyFramebuffers();
    }
}