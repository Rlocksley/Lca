#include "Core.hpp"
#include "Mutex.hpp"
#include "Image.hpp"

namespace Lca
{
namespace Core
{
    void createCore()
    {
        createWindow();
        Time::createTime();
        Input::createInput();

        createInstance();
        createSurface();
        pickPhysicalDevice();
        createDevice();
        getQueue();
        createAllocator();
        createDescriptorPool();

        for(uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            command[i] = createCommand();
        }

        Core::singleCommand = createSingleCommand();

        //createEye();

        createSwapchain();
        //createRenderPass();
        createFramebuffers();

        //createLightBuffers();
        //createShadowMapArray(10048, 10048, 16, VK_FORMAT_D32_SFLOAT);

        //createDescriptorSetLayouts();
        //createPipelineLayouts();
    }

    void destroyCore()
    {
        vkDeviceWaitIdle(vkDevice);

        //destroyPipelineLayouts();
        //destroyDescriptorSetLayouts();

        //destroyShadowMapArray();
        //destroyLightBuffers();
        
        destroyFramebuffers();
        //destroyRenderPass();
        destroySwapchain();

        //destroyEye();

        destroySingleCommand(Core::singleCommand);   
        for(uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            destroyCommand(command[i]);
        }

        
        destroyDescriptorPool();
        destroyAllocator();
        destroyDevice();
        destroySurface();
        destroyInstance();

        destroyWindow();
    }

    bool updateCore()
    {
        static int frameCounter = 1;
        int framerate = static_cast<int>(1.f / Time::deltaTime);
        if(frameCounter++%1000 == 0 || framerate < 100){
            std::cout << "Framerate: " << framerate << " FPS" << std::endl;
        };

        // Wait for the GPU to finish with this frame index's resources before
        // the main thread overwrites its staging buffers during world.progress().
        // This is what provides real double-buffer pipelining: the fence for
        // frame N blocks here while the GPU executes frame N^1 (the other index).
        Lca::Core::waitForCommand(Lca::Core::command[currentFrameIndex]);

        glfwPollEvents();
        
        Time::updateTime();
        Input::updateInput();

        return !glfwWindowShouldClose(pGLFWwindow);
    }

}
}