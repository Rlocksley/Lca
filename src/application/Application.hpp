#pragma once

#include "Core.hpp"
#include "Renderer.hpp"
#include "AssetManager.hpp"
#include "flecs.h"
#include "Level.hpp"
#include <thread>
#include <mutex>
#include <atomic>
#include <string>
#include <memory>
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <condition_variable>

namespace Lca {

class Application {
public:
    Application(const std::string& title, int width, int height);
    virtual ~Application();

    // Start the application loop
    void run();

    // Request to load a level directly (uses loading screen)
    void loadLevel(std::shared_ptr<Level> level);

    // ── Level Streaming API (no loading screen) ─────────────────────
    // Request background load + seamless integration of a streaming level.
    void streamIn(std::shared_ptr<StreamingLevel> level);

    // Request removal of a streaming level by its ID.
    void streamOut(const std::string& levelId);

    // Stream out every active streaming level and cancel pending loads.
    void streamOutAll();

    // Query whether a streaming level is fully integrated.
    bool isStreamedIn(const std::string& levelId) const;

    // Query whether a streaming level is currently loading in the background.
    bool isStreamingIn(const std::string& levelId) const;

protected:
    // Called once at startup
    virtual void onInit() {}
    
    // Called when a level starts loading. Set up your loading screen entities here.
    // The worldMutex is already locked when this is called.
    virtual void onLoadingScreenSetup() {}
    
    // Called when loading is complete and the level is ready to play.
    // The worldMutex is already locked when this is called.
    // Note: All non-persistent entities (including loading screen entities) 
    // will be automatically destroyed right after this function returns.
    virtual void onLevelReady() {}
    
    // Called every frame before world.progress()
    virtual void onUpdate() {}

    // Called on main thread when a streaming level finishes loading and
    // has been integrated into the scene.
    virtual void onStreamingLevelLoaded(const std::string& levelId) {}

    // Called on main thread after a streaming level has been removed
    // from the scene.
    virtual void onStreamingLevelUnloaded(const std::string& levelId) {}

    flecs::world world;
    std::recursive_mutex worldMutex;

private:
    void loadingWorker(std::shared_ptr<Level> level);

    std::atomic<bool> isLoading;
    std::atomic<bool> loadingComplete;
    std::shared_ptr<Level> pendingLevel;
    std::shared_ptr<Level> currentLevel;

    std::thread loadingThread;

    // ── Level Streaming internals ───────────────────────────────────
    void startStreamingWorker();
    void stopStreamingWorker();
    void streamingWorkerFunc();
    void processStreaming();       // main-thread integration step
    void cancelPendingStreams();    // cancel all queued/in-progress streams

    std::thread                     streamingWorkerThread;
    mutable std::mutex              streamingMutex;
    std::condition_variable         streamingCV;

    // Queued for background loading
    std::queue<std::shared_ptr<StreamingLevel>>  streamInQueue;
    // Loaded on background thread, awaiting main-thread integration
    std::vector<std::shared_ptr<StreamingLevel>> streamReadyQueue;
    // IDs queued for removal on the main thread
    std::vector<std::string>                     streamOutQueue_;

    // Currently-active streaming levels (main-thread only)
    std::unordered_map<std::string, std::shared_ptr<StreamingLevel>> activeStreamingLevels;
    // IDs that are queued or being loaded (mutex-protected)
    std::unordered_set<std::string> pendingStreamIds;
    // IDs whose load should be discarded on completion (mutex-protected)
    std::unordered_set<std::string> cancelledStreamIds;

    std::atomic<bool> streamingWorkerRunning;
};

} // namespace Lca
