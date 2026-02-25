#include "Application.hpp"
#include "Time.hpp"
#include "Input.hpp"
#include "Mesh.hpp"
#include "Hidden.hpp"
#include "Persistent.hpp"
#include "Device.hpp"
#include <iostream>

namespace Lca {

Application::Application(const std::string& title, int width, int height) 
    : isLoading(false), loadingComplete(false), pendingLevel(nullptr), currentLevel(nullptr) {
    Core::windowTitle = title;
    Core::windowWidth = width;
    Core::windowHeight = height;
    Core::vkSampleCountFlagBits = VK_SAMPLE_COUNT_4_BIT;    
    Core::createCore();
    Core::GetAssetManager().init();
    Core::GetRenderer().init();
}

Application::~Application() {
    if (loadingThread.joinable()) {
        loadingThread.join();
    }
    vkDeviceWaitIdle(Core::vkDevice);
    Core::GetAssetManager().shutdown();
    Core::GetRenderer().shutdown();
    Core::destroyCore();
}

void Application::loadLevel(std::shared_ptr<Level> level) {
    if (isLoading.load() || !level) return;

    pendingLevel = level;
    isLoading.store(true);
    loadingComplete.store(false);

    {
        std::lock_guard<std::recursive_mutex> lock(worldMutex);
        
        // 1. Hide persistent entities so they don't render during loading
        world.defer_begin();
        world.each<Lca::Component::Persistent>([](flecs::entity e, const Lca::Component::Persistent& per) {
            e.add<Component::Hidden>();
        });
        world.defer_end();

        // 2. Cleanup old level entities
        if (currentLevel) {
            currentLevel->cleanupScene(world);
        }

        // 3. Setup loading screen entities
        onLoadingScreenSetup();
    }

    if (loadingThread.joinable()) {
        loadingThread.join();
    }
    loadingThread = std::thread(&Application::loadingWorker, this, level);
}

void Application::loadingWorker(std::shared_ptr<Level> level) {
    // Call user-defined load function (runs in background)
    level->loadAssets();
    loadingComplete.store(true);
}

void Application::run() {
    onInit();

    std::atomic<bool> renderHasWork(false);
    std::atomic<bool> renderShouldStop(false);
    std::atomic<uint32_t> renderFrameIndex(0);

    std::thread renderThread([&]() {
        while(true) {
            while(!renderHasWork.load(std::memory_order_acquire)) {
                if(renderShouldStop.load(std::memory_order_acquire)) {
                    return;
                }
                std::this_thread::yield();
            }

            const uint32_t frameIndex = renderFrameIndex.load(std::memory_order_relaxed);

            Core::GetRenderer().recordFrame(frameIndex);
            Core::GetRenderer().submitFrame(frameIndex);

            renderHasWork.store(false, std::memory_order_release);
        }
    });

    while (Core::updateCore()) {
        const uint32_t frameIndex = Core::currentFrameIndex;

        {
            std::lock_guard<std::recursive_mutex> lock(worldMutex);
            
            // Check if a system requested a level load
            if (world.has<LoadLevelRequest>()) {
                auto& req = world.get_mut<LoadLevelRequest>();
                if (req.nextLevel && !isLoading.load()) {
                    loadLevel(req.nextLevel);
                }
                world.remove<LoadLevelRequest>();
            }
        }

        if (isLoading.load() && loadingComplete.load()) {
            if (loadingThread.joinable()) {
                loadingThread.join();
            }

            // Wait for the render thread to finish its current frame
            while(renderHasWork.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }

            // Wait for the GPU to finish all submitted work before
            // destroying any Vulkan resources (buffers, pipelines, etc.)
            vkDeviceWaitIdle(Core::vkDevice);
            
            {
                std::lock_guard<std::recursive_mutex> lock(worldMutex);
                
                // 1. Cleanup loading screen
                onLevelReady();

                // 2. Show persistent entities again
                world.defer_begin();
                world.each<Lca::Component::Persistent>([](flecs::entity e, const Lca::Component::Persistent& per) {
                    e.remove<Lca::Component::Hidden>();
                });
                world.defer_end();
                
                // 3. Delete all non-persistent user entities (loading screen)
                Level::deleteNonPersistentEntities(world);

                // 4. Update pipeline descriptor sets now that new assets are loaded
                //    (must happen on the main thread after GPU is idle)
                Core::GetRenderer().updatePipelineDescriptorSets();

                // 5. Setup new level scene
                pendingLevel->setupScene(world);
                currentLevel = pendingLevel;
                pendingLevel = nullptr;
            }
            
            isLoading.store(false);
            loadingComplete.store(false);
        }

        onUpdate();

        {
            std::lock_guard<std::recursive_mutex> lock(worldMutex);
            world.progress();
        }

        while(renderHasWork.load(std::memory_order_acquire)) {
            std::this_thread::yield();
        }

        renderFrameIndex.store(frameIndex, std::memory_order_relaxed);
        renderHasWork.store(true, std::memory_order_release);

        Core::currentFrameIndex = (Core::currentFrameIndex + 1) % Core::MAX_FRAMES_IN_FLIGHT;
    }

    while(renderHasWork.load(std::memory_order_acquire)) {
        std::this_thread::yield();
    }

    renderShouldStop.store(true, std::memory_order_release);
    renderThread.join();
}

} // namespace Lca
