#pragma once

#include "Global.hpp"
#include "Image.hpp"

namespace Lca
{
    namespace Core
    {
        struct Framebuffers
        {
            Image colorImage;
            Image depthImage;
            std::vector<VkFramebuffer> vkFramebuffers{};
        };

        inline Framebuffers framebuffers;

        void createFramebuffers();
        void destroyFramebuffers();
    }
}