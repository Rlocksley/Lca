#pragma once
#include <iostream>
#include <memory>
#include <functional>
#include <string>
#include <thread>
#include <atomic>
#include <chrono>

#include "Global.hpp"
#include "Core.hpp"
#include "flecs.h"
#include "Level.hpp"
#include "GameWorld.hpp"
#include "LoadingScreen.hpp"


// --- The Application Class (Multi-World Version) ---
namespace Rx{
    class GameWorld;
    class LoadingScreen;
    class Application {
    public:
        // Application state to control the main loop
        enum class State {
            LOADING,
            RUNNING_GAME,
            QUITTING
        };

        // Level class remains the same abstract interface
        

        Application();

        ~Application();

        template<typename Comp>
        Application& registerComponent(const std::string& name);


        template <typename GW, typename FirstLevel>
        Application& addGameWorld();

        template <typename LS>
        Application& addLoadingWorld();

        // The main application loop, now a state machine.
        void run();
        void setState(State newState);

    private:
        flecs::world game;
        flecs::world loading;
        // --- State-specific loop functions ---

        void runLoading();

        void runGame();

        

        // --- Member Variables ---
        std::unique_ptr<GameWorld> gameWorld{ nullptr };
        std::unique_ptr<GameWorldBase> loadingWorld{ nullptr };
        
        // State management
        State state;

    };
}


namespace Rx{

    template<typename Comp>
    Application& Application::registerComponent(const std::string& name){
        game.component<Comp>(name.c_str());
        loading.component<Comp>(name.c_str());
        return *this;
    }

     template <typename GW, typename FirstLevel>
        Application& Application::addGameWorld() {
                static_assert(std::is_base_of<GameWorld, GW>::value, "GameWorld must be a subclass of Rx::GameWorld");
                gameWorld = std::make_unique<GW>(*this, game);
                std::cout << "[Application] Game World created." << std::endl;
                // Set the initial level to load.
                gameWorld->requestInitialLevel<FirstLevel>();

                return *this;
            }

        template <typename LS>
        Application& Application::addLoadingWorld() {
            static_assert(std::is_base_of<LoadingScreen, LS>::value, "LoadingScreen must be a subclass of Rx::LoadingScreen");
            loadingWorld = std::make_unique<LS>(*this, loading);
            std::cout << "LoadingWorld::loadGlobal." << std::endl;
            loadingWorld->loadGlobal();

            return *this;
        }
}