#pragma once

#include "Global.hpp"

namespace Lca
{
    namespace Core
    {
        VkFence createFence(VkFenceCreateFlags flags);
        void destroyFence(VkFence fence);
    }
}