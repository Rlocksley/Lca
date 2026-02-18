#include "Allocator.hpp"
#include "Instance.hpp"
#include "PhysicalDevice.hpp"
#include "Device.hpp"

#define VMA_IMPLEMENTATION
#include "vma/vk_mem_alloc.h"

namespace Lca
{
    namespace Core
    {
        void createAllocator()
        {
            VmaAllocatorCreateInfo createInfo{};
            createInfo.vulkanApiVersion = VK_API_VERSION_1_3;
            createInfo.instance = vkInstance;
            createInfo.physicalDevice = vkPhysicalDevice;
            createInfo.device = vkDevice;
            //createInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
            createInfo.flags = VMA_ALLOCATOR_CREATE_EXTERNALLY_SYNCHRONIZED_BIT;
            LCA_CHECK_VULKAN
            (vmaCreateAllocator
            (&createInfo,&vmaAllocator),
            "createAllocator",
            "vmaCreateAllocator");

            #ifdef LCA_DEBUG
            LCA_LOGI("Allocator", "created", "")
            #endif
        }

        void destroyAllocator()
        {
            vmaDestroyAllocator(vmaAllocator);
        }
    }
}