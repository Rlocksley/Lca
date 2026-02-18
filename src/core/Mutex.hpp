#pragma once
#include <mutex>

namespace Lca{
    namespace Core{
    inline std::mutex vulkanCommandMutex; // Global mutex for thread safety
}
}

#define LCA_VK_MUTEX(Expression)\
{\
    Lca::Core::vulkanCommandMutex.lock();\
    Expression;\
    Lca::Core::vulkanCommandMutex.unlock();\
}
