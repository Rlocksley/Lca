#pragma once

#include "Global.hpp"

namespace Lca
{
    namespace Core
    {
        struct Command
        {
            VkCommandPool vkCommandPool;
            VkCommandBuffer vkCommandBuffer;
            VkFence vkFence;
        };

        struct SingleCommand
        {
            VkCommandPool vkCommandPool;
            VkCommandBuffer vkCommandBuffer;
            VkFence vkFence;
        };

        const inline uint32_t MAX_FRAMES_IN_FLIGHT = 2;
        inline uint32_t currentFrameIndex = 0;
        inline Lca::Core::SingleCommand singleCommand;
        inline std::array<Lca::Core::Command, MAX_FRAMES_IN_FLIGHT> command;

        Command createCommand();
        void destroyCommand(Command command);

        void beginCommand(Command command);
        void endCommand(Command command);
        void waitForCommand(Command command);

        SingleCommand createSingleCommand();
        void destroySingleCommand(SingleCommand singleCommand);

        void beginSingleCommand(SingleCommand singleCommand);
        void endSingleCommand(SingleCommand singleCommand);
 
   }
}