#pragma once
#include "Level.hpp"
#include "AssetManager.hpp"
#include "Renderer.hpp"
#include "Helpers.hpp"
#include "Shape.hpp"
#include "Mesh.hpp"
#include "FlyingCamera.hpp"
#include "Light.hpp"
#include "Time.hpp"
using namespace Lca;
struct LightVelocity {
    glm::vec3 velocity{0.0f};
};
class GameLevel : public Lca::Level {

public:
    void loadAssets() override {
        // Load the Sponza model via assimp
        Core::GetAssetManager().loadModel("sponza", "C:\\Users\\robry\\Desktop\\3DModels\\sponza-palace\\source\\scene.glb");
    }

    void setupScene(flecs::world& world) override {
        
        world.entity("Player")
        .set<Lca::Component::FlyingCamera>({
            .position = glm::vec3(0.0f, 5.0f, 0.0f),
            .angle = glm::vec2(0.0f, 0.0f),
            .speed = 10.0f,
            .rotationSpeed = 0.005f,
            .fov = glm::radians(60.0f),
            .nearClip = 0.1f,
            .farClip = 1000.0f
        })
        .add<Lca::Component::Transform>();

        // Create the Sponza model entity using the new Model API
        const auto& sponzaModel = Core::GetAssetManager().getModel("sponza");
        uint32_t pipelineId = Core::GetRenderer().getMeshPipelineId("basic");

        auto sponza = Lca::Helpers::createModel(world, sponzaModel, pipelineId);
        sponza.set_name("Sponza");
        sponza.set(Component::Transform{glm::vec3(0.0f), 0.0f, glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.01f)});
        sponza.add<Component::Static>();

        // Create 100 random point lights
        for (uint32_t i = 0; i < 500; i++) {
            auto light = world.entity(("pointlight_" + std::to_string(i)).c_str());
            light.set(Component::Transform{
                20.f * glm::vec3(random(-1.0f, 1.0f), random(-1.0f, 1.0f), random(-1.0f, 1.0f)),
                0.0f,
                glm::vec3(0.0f, 1.0f, 0.0f),
                glm::vec3(1.0f)
            });
            auto mod3 = i % 3;
            light.set(Component::PointLight{
                .color     = glm::vec3(mod3 == 0 ? 1.0f : 0.0f, mod3 == 1 ? 1.0f : 0.0f, mod3 == 2 ? 1.0f : 0.0f),
                .intensity = random(800.0f, 800.0f),
                .radius    = random(11.0f, 11.0f)
            });
            light.set(LightVelocity{
                20.f*glm::vec3(random(-1.0f, 1.0f), random(-1.0f, 1.0f), random(-1.0f, 1.0f))
            });
        }

        // System: move point lights randomly, bounce off bounds
        world.system<Component::Transform, LightVelocity>("Light Movement")
            .with<Component::PointLight>()
            .each([](flecs::entity e, Component::Transform& t, LightVelocity& vel) {
                constexpr float BOUND = 20.0f;
                t.position += vel.velocity * Time::deltaTime;

                // Bounce off walls
                for (int axis = 0; axis < 3; axis++) {
                    if (t.position[axis] > BOUND) {
                        t.position[axis] = BOUND;
                        vel.velocity[axis] = -vel.velocity[axis];
                    } else if (t.position[axis] < -BOUND) {
                        t.position[axis] = -BOUND;
                        vel.velocity[axis] = -vel.velocity[axis];
                    }
                }

            });
    }
};
