#include "Application.hpp"
#include "Input.hpp"
namespace Rx{
        Application::Application() : 
                state(State::LOADING)
            {
                Rx::Core::createCore();
            }

        Application::~Application() {
                gameWorld->getWorld().reset(); // Reset the game world before destruction
                loadingWorld->getWorld().reset(); // Reset the loading world before destruction
                Rx::Core::destroyCore(); // Clean up the core resources
                std::cout << "[Application] Resources cleaned up." << std::endl;
            }


       

        // The main application loop, now a state machine.
        void Application::run() {
            std::cout << "--- Application starting ---" << std::endl;

            while (Rx::Core::updateCore() && state != State::QUITTING) {
                if(1.f/Rx::Time::deltaTime < 300.f) {
                    std::cout << "Framerate: " << 1.f/Rx::Time::deltaTime << std::endl;
                }
                Rx::Core::getSwapchainImageIndex();

                switch (state) {
                    case State::LOADING:
                        runLoading();
                        break;
                    case State::RUNNING_GAME:
                        runGame();
                        break;
                    case State::QUITTING:
                        // Loop will terminate
                        break;
                }
            }
            vkDeviceWaitIdle(Rx::Core::vkDevice);         
            std::cout << "\n--- Application ended ---" << std::endl;
        }

        void Application::setState(State newState) {
            state = newState;
        }

        // --- State-specific loop functions ---

        void Application::runLoading() {
            // Progress the loading screen world.
            loadingWorld->getWorld().progress();
            
            // Check if the background thread has finished loading the game world.
            if (gameWorld->getIsLoadComplete().load()) {
                std::cout << "\n[Main Thread] Load complete! Joining background thread..." << std::endl;
                gameWorld->getLoadingThread().join(); // Clean up the finished thread.

                std::cout << "[Main Thread] Switching to Game World." << std::endl;
                state = State::RUNNING_GAME;
            }
        }

        void Application::runGame() {
            // Progress the main game world.
            gameWorld->getWorld().progress();
                
            if(gameWorld->getShouldQuit()) {
                std::cout << "[Game World] Quit requested. Exiting main loop." << std::endl;
                state = State::QUITTING;
            } else if (gameWorld->getShouldSwitch()) {
                std::cout << "[Game World] Level switch requested. Switching levels..." << std::endl;
                gameWorld->beginLevelLoad();
                state = State::LOADING;
            }
        }
}