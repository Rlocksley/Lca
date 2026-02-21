#include "Application.hpp"
#include "Time.hpp"
#include "Input.hpp"
#include "Mesh.hpp"
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
        world.each<Persistent>([](flecs::entity e, Persistent& per) {
            e.add<Component::Hidden>();
        });
        world.defer_end();

        // 2. Cleanup old level (deletes non-persistent entities)
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
            renderHasWork.store(false, std::memory_order_release);

            Core::GetRenderer().recordFrame(frameIndex);
            Core::GetRenderer().submitFrame(frameIndex);
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
            
            {
                std::lock_guard<std::recursive_mutex> lock(worldMutex);
                
                // 1. Cleanup loading screen
                onLevelReady();

                // 2. Show persistent entities again
                world.defer_begin();
                world.each<Persistent>([](flecs::entity e, Persistent& per) {
                    e.remove<Component::Hidden>();
                });
                
                // Delete all entities that do not have the Persistent tag (e.g. loading screen entities)
                auto q = world.query_builder<>().without<Persistent>().build();
                q.each([](flecs::entity e) {
                    e.destruct();
                });
                world.defer_end();

                // 3. Setup new level scene
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
