#pragma once
#include "Level.hpp"
#include "AssetManager.hpp"
#include "Renderer.hpp"
#include "Shape.hpp"
#include "Mesh.hpp"
#include "FlyingCamera.hpp"
#include "Light.hpp"
#include "Time.hpp"
using namespace Lca;


#define STREAMING_DISTANCE 20.0f

// Per-light velocity for wandering point lights
struct LightVelocity {
    glm::vec3 velocity{0.0f};
};

struct CubeTag{};
struct SphereTag{};
struct IsVisible{};
struct MeshChild { flecs::entity child; };

class GameLevel : public Lca::Level {

public:
    void loadAssets() override {
        
        std::this_thread::sleep_for(std::chrono::seconds(2)); // Simulate loading time


        Shape::Cube cube2(glm::vec3(1.0f), glm::vec3(10.0f), glm::vec4(1.0f));
        Core::GetAssetManager().addMesh("cube2", cube2.getVertices(), cube2.getIndices());

        Shape::Sphere sphere(1, 16, 16, glm::vec3(10.0f), glm::vec4(1.0f));
        Core::GetAssetManager().addMesh("sphere", sphere.getVertices(), sphere.getIndices());

        Core::GetAssetManager().loadTexture("fire", "asset/fire.jpg");

        Core::Material material{};
        material.metallic = 1.0f;
        material.roughness = 0.5f;
        material.textureId = Core::GetAssetManager().getTextureId("fire");

        Core::GetAssetManager().addMaterial("fire_material", material);



    }

    void setupScene(flecs::world& world) override {
        
        world.entity("Player")
        .set<Lca::Component::FlyingCamera>({
            .position = glm::vec3(0.0f, 0.0f, 5.0f),
            .angle = glm::vec2(0.0f, 0.0f),
            .speed = 10.0f,
            .rotationSpeed = 0.005f,
            .fov = glm::radians(60.0f),
            .nearClip = 0.1f,
            .farClip = 1000.0f
        })
        .add<Lca::Component::Transform>();

        world.system<CubeTag, Component::Transform>("Cube Streaming")
            .without<IsVisible>()
            .run([&world](flecs::iter& it) {
                
                auto& playerTransform = it.world().entity("Player").get<Component::Transform>();
                while(it.next()) {
                    for (auto i : it) {
                        auto e = it.entity(i);
                        const auto& t = it.field<Component::Transform>(1);
                        if (glm::length(t[i].position - playerTransform.position) < STREAMING_DISTANCE) {
                            e.add<IsVisible>();
                            // Create an unnamed child entity, attach to parent, and store it on the parent
                            flecs::entity meshEntity = it.world().entity();
                            meshEntity.add(flecs::ChildOf, e);
                            meshEntity.set<Component::Mesh>({
                                Core::GetAssetManager().getMeshId("cube"),
                                Core::GetAssetManager().getMaterialId("fire_material"),
                                Core::GetRenderer().getMeshPipelineId("basic")
                            });
                        }
            
                    }
                }
               
            });

        world.system<IsVisible, Component::Transform>("Cube Unstreaming")
            .with<CubeTag>()
            .run([](flecs::iter& it) {
                auto playerTransform = it.world().entity("Player").get<Component::Transform>();
                while(it.next()) {
                    for (auto i : it) {
                        auto e = it.entity(i);
                        const auto& t = it.field<Component::Transform>(1);
                        if (glm::length(t[i].position - playerTransform.position) >= STREAMING_DISTANCE) {
                            e.children([](flecs::entity entity){
                                if (entity.has<Component::Mesh>()) {
                                    entity.destruct();
                                }
                            });
                            e.remove<IsVisible>();
                        }
                    }
                }
            });




        init_random();
        uint32_t numModels = 50000;
        for (uint32_t i = 0; i < numModels; i++) {

            auto model = world.entity();
            model.set(Component::Transform{glm::vec3(random(-100.0f, 100.0f), random(-100.0f, 100.0f), random(-100.0f, 100.0f)), 0.0f, glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(1.0f)});
            model.add<Component::TransformID>();
            model.add<Component::Static>();
            model.add<CubeTag>();

            // auto cube = world.entity(("cube_" + std::to_string(i)).c_str());
            // cube.add(flecs::ChildOf, model);
            // cube.set(Component::Mesh{
            //             Core::GetAssetManager().getMeshId("cube"),
            //             Core::GetAssetManager().getMaterialId("fire_material"),
            //             Core::GetRenderer().getMeshPipelineId("basic")
            //         });

            // auto sphere = world.entity(("sphere_" + std::to_string(i)).c_str());
            // sphere.add(flecs::ChildOf, model);
            // sphere.set(Component::Mesh{
            //             Core::GetAssetManager().getMeshId("sphere"),
            //             Core::GetAssetManager().getMaterialId("fire_material"),
            //             Core::GetRenderer().getMeshPipelineId("basic")
            //         });
            
        }

        // Create 1000 random point lights
        for (uint32_t i = 0; i < 1000; i++) {
            auto light = world.entity(("pointlight_" + std::to_string(i)).c_str());
            light.set(Component::Transform{
                glm::vec3(random(-100.0f, 100.0f), random(-100.0f, 100.0f), random(-100.0f, 100.0f)),
                0.0f,
                glm::vec3(0.0f, 1.0f, 0.0f),
                glm::vec3(1.0f)
            });
            auto mod3 = i % 3;
            light.set(Component::PointLight{
                .color     = glm::vec3(mod3 == 0 ? 1.0f : 0.0f, mod3 == 1 ? 1.0f : 0.0f, mod3 == 2 ? 1.0f : 0.0f),
                .intensity = random(30.0f, 30.0f),
                .radius    = random(200.0f, 200.0f)
            });
            light.set(LightVelocity{
                glm::vec3(random(-20.0f, 20.0f), random(-20.0f, 20.0f), random(-20.0f, 20.0f))
            });
        }

        // System: move point lights randomly, bounce off the [-100,100]³ bounds
        world.system<Component::Transform, LightVelocity>("Light Movement")
            .with<Component::PointLight>()
            .each([](flecs::entity e, Component::Transform& t, LightVelocity& vel) {
                constexpr float BOUND = 100.0f;
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