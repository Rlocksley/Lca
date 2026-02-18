#pragma once

#include "Global.hpp"


namespace Lca
{
    namespace Core
    {
        
        inline std::string windowTitle;
        inline uint32_t windowWidth;
        inline uint32_t windowHeight;
        inline bool fullScreen;
        inline GLFWwindow* pGLFWwindow;

        void createWindow();
        void destroyWindow();
   }
}