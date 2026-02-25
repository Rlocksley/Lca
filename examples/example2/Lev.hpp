#pragma once
#include "Level.hpp"
#include "AssetManager.hpp"
#include "Renderer.hpp"
#include "Shape.hpp"
#include "Mesh.hpp"
#include "FlyingCamera.hpp"
using namespace Lca;
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
        
        world.entity().set<Lca::Component::FlyingCamera>({
            .position = glm::vec3(0.0f, 0.0f, 5.0f),
            .angle = glm::vec2(0.0f, 0.0f),
            .speed = 10.0f,
            .rotationSpeed = 0.005f,
            .fov = glm::radians(60.0f),
            .nearClip = 0.1f,
            .farClip = 1000.0f
        });
        
        init_random();
        uint32_t numModels = 50000;
        for (uint32_t i = 0; i < numModels; i++) {

            auto model = world.entity();
            model.set(Component::Transform{glm::vec3(random(-100.0f, 100.0f), random(-100.0f, 100.0f), random(-100.0f, 100.0f)), 0.0f, glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(1.0f)});
            model.add<Component::TransformID>();
            model.add<Component::Static>();

            auto cube = world.entity(("cube_" + std::to_string(i)).c_str());
            cube.add(flecs::ChildOf, model);
            cube.set(Component::Mesh{
                        Core::GetAssetManager().getMeshId("cube"),
                        Core::GetAssetManager().getMaterialId("fire_material"),
                        Core::GetRenderer().getMeshPipelineId("basic")
                    });

            auto sphere = world.entity(("sphere_" + std::to_string(i)).c_str());
            sphere.add(flecs::ChildOf, model);
            sphere.set(Component::Mesh{
                        Core::GetAssetManager().getMeshId("sphere"),
                        Core::GetAssetManager().getMaterialId("fire_material"),
                        Core::GetRenderer().getMeshPipelineId("basic")
                    });
            
        }
    }
};