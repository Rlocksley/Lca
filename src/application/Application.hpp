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

namespace Lca {

class Application {
public:
    Application(const std::string& title, int width, int height);
    virtual ~Application();

    // Start the application loop
    void run();

    // Request to load a level directly
    void loadLevel(std::shared_ptr<Level> level);

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

    flecs::world world;
    std::recursive_mutex worldMutex;

private:
    void loadingWorker(std::shared_ptr<Level> level);

    std::atomic<bool> isLoading;
    std::atomic<bool> loadingComplete;
    std::shared_ptr<Level> pendingLevel;
    std::shared_ptr<Level> currentLevel;

    std::thread loadingThread;
};

} // namespace Lca
