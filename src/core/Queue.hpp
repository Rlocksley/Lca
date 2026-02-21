#pragma once

#include "Global.hpp"
#include "Command.hpp"

namespace Lca
{
    namespace Core
    {
        inline VkQueue vkQueue;

        void getQueue();
        void submitSingleCommand(SingleCommand command);
        void submitGraphicsCommand(Command command, uint32_t frameIndex);
        void presentGraphics(Command command, uint32_t frameIndex);
    }
}