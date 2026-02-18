#pragma once

#include "Global.hpp"

namespace Lca
{
    namespace Core
    {
        inline VkSurfaceKHR vkSurfaceKHR;

        void createSurface();
        void destroySurface();
    }
}