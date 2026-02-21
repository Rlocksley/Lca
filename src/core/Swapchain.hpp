#pragma once

#include "Global.hpp"
#include "Command.hpp"

namespace Lca
{
    namespace Core
    {
        struct Swapchain
        {
            std::vector<VkImage> vkImages{};
            std::vector<VkImageView> vkImageViews{};
            VkSwapchainKHR vkSwapchainKHR;
            std::array<VkSemaphore, MAX_FRAMES_IN_FLIGHT> imageAvailableSemaphores{};  // One per swapchain image
            std::vector<VkSemaphore> renderFinishedSemaphores{};  // One per swapchain image
            uint32_t imageIndex;
        };

        inline Swapchain swapchain;

        void createSwapchain();
        void destroySwapchain();
        void getSwapchainImageIndex(uint32_t frameIndex);
    }
}