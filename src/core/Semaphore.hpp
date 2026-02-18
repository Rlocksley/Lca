#pragma once

#include "Global.hpp"

namespace Lca
{
    namespace Core
    {
        VkSemaphore createSemaphore();
        void destroySemaphore(VkSemaphore semaphore);
    }
}