#include "Application.hpp"
#include "Time.hpp"
#include "Input.hpp"
#include "Mesh.hpp"
#include "Hidden.hpp"
#include "Persistent.hpp"
#include "Device.hpp"
#include <iostream>
#include <condition_variable>
#include <chrono>
#include <thread>

namespace Lca {

Application::Application(const std::string& title, int width, int height) 
    : isLoading(false), loadingComplete(false), pendingLevel(nullptr), currentLevel(nullptr),
      streamingWorkerRunning(false) {
    // Enable flecs multi-threading for systems. Reserve one core for main/render.
    {
        unsigned int hc = std::thread::hardware_concurrency();
        unsigned int threads = (hc <= 1) ? 1u : (hc - 1);
        world.set_threads(static_cast<int>(threads));
    }
    Core::windowTitle = title;
    Core::windowWidth = width;
    Core::windowHeight = height;
    Core::vkSampleCountFlagBits = VK_SAMPLE_COUNT_4_BIT;    
    Core::createCore();
    Core::GetAssetManager().init();
    Core::GetRenderer().init();
    startStreamingWorker();
}

Application::~Application() {
    stopStreamingWorker();
    if (loadingThread.joinable()) {
        loadingThread.join();
    }
    vkDeviceWaitIdle(Core::vkDevice);
    Core::GetRenderer().shutdown();
    Core::GetAssetManager().shutdown();
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

        // 1b. Cancel pending streaming and clean up active streaming levels
        cancelPendingStreams();
        for (auto& [id, sl] : activeStreamingLevels) {
            sl->cleanupScene(world);
        }
        activeStreamingLevels.clear();

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

    // Condition-variable based sync: threads block when idle instead of
    // burning CPU with yield() spin-loops.  Under screen recording (or any
    // other CPU-heavy background task) the OS can freely schedule the idle
    // thread away without causing frame-timing jitter.
    std::mutex              renderMutex;
    std::condition_variable workReady;    // main  → render: new frame queued
    std::condition_variable workDone;     // render → main:  frame finished
    bool     renderHasWork    = false;
    bool     renderIdle       = true;
    bool     renderShouldStop = false;
    uint32_t renderFrameIndex = 0;

    // Frame rate limiter — 260 FPS cap for both threads (render thread is
    // gated by the main thread handoff, so capping here caps both).
    constexpr std::chrono::duration<double> targetFrameTime(1.0 / 260.0);

    std::thread renderThread([&]() {
        while(true) {
            uint32_t frameIndex;
            {
                std::unique_lock<std::mutex> lock(renderMutex);
                workReady.wait(lock, [&]{ return renderHasWork || renderShouldStop; });
                if(renderShouldStop) return;
                frameIndex = renderFrameIndex;
            }

            // Fence wait already happened on the main thread (in updateCore)
            // before staging buffers were written, so command[frameIndex] is
            // safe to reuse here.
            Core::GetRenderer().recordFrame(frameIndex);
            Core::GetRenderer().submitFrame(frameIndex);

            {
                std::lock_guard<std::mutex> lock(renderMutex);
                renderHasWork = false;
                renderIdle    = true;
            }
            workDone.notify_one();
        }
    });

    while (Core::updateCore()) {
        auto frameStart = std::chrono::high_resolution_clock::now();
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

            // Check if a system requested streaming level loads
            if (world.has<StreamInRequest>()) {
                auto& req = world.get_mut<StreamInRequest>();
                for (auto& sl : req.levels) {
                    streamIn(sl);
                }
                world.remove<StreamInRequest>();
            }

            // Check if a system requested streaming level unloads
            if (world.has<StreamOutRequest>()) {
                auto& req = world.get_mut<StreamOutRequest>();
                for (auto& id : req.levelIds) {
                    streamOut(id);
                }
                world.remove<StreamOutRequest>();
            }
        }

        if (isLoading.load() && loadingComplete.load()) {
            if (loadingThread.joinable()) {
                loadingThread.join();
            }

            // Block until the render thread has finished its current frame
            // (CV wait: truly sleeps, does not spin-burn CPU).
            {
                std::unique_lock<std::mutex> lock(renderMutex);
                workDone.wait(lock, [&]{ return renderIdle; });
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

        // Wait for render thread to finish last frame, then hand it the new one.
        {
            std::unique_lock<std::mutex> lock(renderMutex);
            workDone.wait(lock, [&]{ return renderIdle; });

            // Integrate completed streaming levels while the render thread is idle.
            // Skip during a full level load (streaming state is already cleared).
            if (!isLoading.load()) {
                bool hasStreamIns = false;
                bool hasStreaming  = false;
                {
                    std::lock_guard<std::mutex> slock(streamingMutex);
                    hasStreamIns = !streamReadyQueue.empty();
                    hasStreaming  = hasStreamIns || !streamOutQueue_.empty();
                }
                if (hasStreaming) {
                    // Both stream-ins (new GPU resources) and stream-outs
                    // (destroying textures/buffers) require the GPU to be
                    // idle before modifying or freeing Vulkan resources.
                    vkDeviceWaitIdle(Core::vkDevice);
                    std::lock_guard<std::recursive_mutex> wlock(worldMutex);
                    processStreaming();
                }
            }

            renderFrameIndex = frameIndex;
            renderHasWork    = true;
            renderIdle       = false;
        }
        workReady.notify_one();

        // Sleep for the remaining frame budget to cap at 260 FPS.
        auto frameEnd = std::chrono::high_resolution_clock::now();
        auto elapsed  = frameEnd - frameStart;
        if (elapsed < targetFrameTime) {
            std::this_thread::sleep_for(targetFrameTime - elapsed);
        }

        Core::currentFrameIndex = (Core::currentFrameIndex + 1) % Core::MAX_FRAMES_IN_FLIGHT;
    }

    // Wait for the last submitted frame to finish, then stop the render thread.
    {
        std::unique_lock<std::mutex> lock(renderMutex);
        workDone.wait(lock, [&]{ return renderIdle; });
        renderShouldStop = true;
    }
    workReady.notify_one();
    renderThread.join();
}

// ─── Level Streaming implementation ─────────────────────────────────────────

void Application::startStreamingWorker() {
    streamingWorkerRunning.store(true);
    streamingWorkerThread = std::thread(&Application::streamingWorkerFunc, this);
}

void Application::stopStreamingWorker() {
    {
        std::lock_guard<std::mutex> lock(streamingMutex);
        streamingWorkerRunning.store(false);
    }
    streamingCV.notify_one();
    if (streamingWorkerThread.joinable()) {
        streamingWorkerThread.join();
    }
}

void Application::streamingWorkerFunc() {
    while (true) {
        std::shared_ptr<StreamingLevel> level;
        {
            std::unique_lock<std::mutex> lock(streamingMutex);
            streamingCV.wait(lock, [this] {
                return !streamInQueue.empty() || !streamingWorkerRunning.load();
            });
            if (!streamingWorkerRunning.load() && streamInQueue.empty()) return;
            if (streamInQueue.empty()) continue;
            level = streamInQueue.front();
            streamInQueue.pop();
        }

        std::string id = level->getId();

        // Check if cancelled while sitting in the queue
        {
            std::lock_guard<std::mutex> lock(streamingMutex);
            if (cancelledStreamIds.count(id)) {
                cancelledStreamIds.erase(id);
                pendingStreamIds.erase(id);
                continue;
            }
        }

        // Heavy work: file I/O, decompression, parsing (background thread)
        level->loadAssets();

        // Check if cancelled during loading
        {
            std::lock_guard<std::mutex> lock(streamingMutex);
            if (cancelledStreamIds.count(id)) {
                cancelledStreamIds.erase(id);
                pendingStreamIds.erase(id);
                continue;
            }
            // Move to ready queue — main thread will integrate it next frame.
            streamReadyQueue.push_back(level);
        }
    }
}

void Application::streamIn(std::shared_ptr<StreamingLevel> level) {
    if (!level) return;
    std::string id = level->getId();
    std::lock_guard<std::mutex> lock(streamingMutex);
    // Skip if already loaded or already queued
    if (activeStreamingLevels.count(id) || pendingStreamIds.count(id)) return;
    pendingStreamIds.insert(id);
    streamInQueue.push(level);
    streamingCV.notify_one();
}

void Application::streamOut(const std::string& levelId) {
    std::lock_guard<std::mutex> lock(streamingMutex);
    // If still pending (queued or loading), just cancel it
    if (pendingStreamIds.count(levelId)) {
        cancelledStreamIds.insert(levelId);
        pendingStreamIds.erase(levelId);
        return;
    }
    // Otherwise queue for main-thread removal
    streamOutQueue_.push_back(levelId);
}

void Application::streamOutAll() {
    std::lock_guard<std::mutex> lock(streamingMutex);
    for (const auto& [id, _] : activeStreamingLevels) {
        streamOutQueue_.push_back(id);
    }
    while (!streamInQueue.empty()) {
        cancelledStreamIds.insert(streamInQueue.front()->getId());
        streamInQueue.pop();
    }
    pendingStreamIds.clear();
    streamReadyQueue.clear();
}

bool Application::isStreamedIn(const std::string& levelId) const {
    return activeStreamingLevels.count(levelId) > 0;
}

bool Application::isStreamingIn(const std::string& levelId) const {
    std::lock_guard<std::mutex> lock(streamingMutex);
    return pendingStreamIds.count(levelId) > 0;
}

void Application::cancelPendingStreams() {
    std::lock_guard<std::mutex> lock(streamingMutex);
    while (!streamInQueue.empty()) {
        cancelledStreamIds.insert(streamInQueue.front()->getId());
        streamInQueue.pop();
    }
    pendingStreamIds.clear();
    streamReadyQueue.clear();
    streamOutQueue_.clear();
}

void Application::processStreaming() {
    // 1. Process stream-out requests
    std::vector<std::string> outRequests;
    {
        std::lock_guard<std::mutex> lock(streamingMutex);
        outRequests.swap(streamOutQueue_);
    }
    for (const auto& id : outRequests) {
        auto it = activeStreamingLevels.find(id);
        if (it != activeStreamingLevels.end()) {
            it->second->cleanupScene(world);
            activeStreamingLevels.erase(it);
            onStreamingLevelUnloaded(id);
        }
    }

    // 2. Integrate completed stream-in requests
    std::vector<std::shared_ptr<StreamingLevel>> ready;
    {
        std::lock_guard<std::mutex> lock(streamingMutex);
        ready.swap(streamReadyQueue);
    }

    // Stream-outs may have destroyed textures whose Vulkan handles are
    // still referenced by pipeline descriptor sets — must rebuild them.
    bool needsDescriptorUpdate = !outRequests.empty();

    for (auto& level : ready) {
        std::string id = level->getId();
        {
            std::lock_guard<std::mutex> lock(streamingMutex);
            pendingStreamIds.erase(id);
        }
        if (activeStreamingLevels.count(id)) continue;

        level->setupScene(world);
        activeStreamingLevels[id] = level;
        needsDescriptorUpdate = true;
        onStreamingLevelLoaded(id);
    }

    if (needsDescriptorUpdate) {
        Core::GetRenderer().updatePipelineDescriptorSets();
    }
}

} // namespace Lca
