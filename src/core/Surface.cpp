#include "Surface.hpp"
#include "Window.hpp"
#include "Instance.hpp"

namespace Lca
{
namespace Core
{
    void createSurface()
    {
        LCA_CHECK_VULKAN
        (glfwCreateWindowSurface
        (vkInstance,
        pGLFWwindow,
        nullptr,
        &vkSurfaceKHR),
        "createSurface",
        "glfwCreateWindowSurface")

        #ifdef LCA_DEBUG
        LCA_LOGI("VkSurfaceKHR" , "created", "")
        #endif
    }

    void destroySurface()
    {
        vkDestroySurfaceKHR
        (vkInstance,
        vkSurfaceKHR,
        nullptr);
    }
}
}