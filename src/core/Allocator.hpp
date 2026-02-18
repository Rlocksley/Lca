#pragma once

#include "Global.hpp"
#include "vma/vk_mem_alloc.h"

namespace Lca
{
    namespace Core
    {
        inline VmaAllocator vmaAllocator;

        void createAllocator();
        void destroyAllocator();
    }
}