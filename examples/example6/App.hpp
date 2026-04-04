#pragma once

#include "Application.hpp"
#include "Light.hpp"
#include "Shape.hpp"
#include "CharacterCamera.hpp"
#include "Lev.hpp"
#include "Input.hpp"
#include <iostream>
#include "PhysicsBody.hpp"
#include "PhysicsModule.hpp"
#include "RigidBody.hpp"
#include "BoxCollider.hpp"
#include "CharacterCapsule.hpp"

// ──────────────────────────────────────────────────────────────
// Example 6 — Level Streaming (10x10 City Grid, proximity-based)
//            + Physics (floor, houses, cars with BoxCollider)
//            + Wizard player character (same as example8)
//
// 100 streaming zones tiled in a 10x10 grid.  An ECS system
// checks the CharacterCamera position every frame and streams in
// zones within range, streaming out zones that are too far away.
//
// WASD to move, Shift to run, Space to jump, Left-click to attack.
// Press 0 to stream out ALL zones at once.
// ──────────────────────────────────────────────────────────────

class ExampleApp : public Lca::Application {
public:
    ExampleApp(const std::string& title, int width, int height)
        : Application(title, width, height) {}

protected:

    void onInit() override {
        using namespace Lca;

        // ── Regular PBR pipeline ──────────────────────────────
        Core::GraphicsPipelineConfig pipelineConfig{};
        pipelineConfig.vertexShader   = "shader/mesh_pbr.vert.spv";
        pipelineConfig.fragmentShader = "shader/mesh_pbr.frag.spv";
        Core::MeshPipeline meshPipeline(pipelineConfig);
        Core::GetRenderer().addMeshPipeline("basic", std::move(meshPipeline), 16384);

        // ── Skeleton PBR pipeline ─────────────────────────────
        Core::GraphicsPipelineConfig skelPipelineConfig{};
        skelPipelineConfig.vertexShader   = "shader/skeleton_mesh_pbr.vert.spv";
        skelPipelineConfig.fragmentShader = "shader/mesh_pbr.frag.spv";
        Core::SkeletonMeshPipeline skelPipeline(skelPipelineConfig);
        Core::GetRenderer().addSkeletonMeshPipeline("skeletonPBR", std::move(skelPipeline), 2*4096);

        
        // Loading-screen cube
        Shape::Cube _cube(glm::vec3(1.0f), glm::vec3(0.0f), glm::vec4(1.0f));
        Core::GetAssetManager().addMesh("cube", _cube.getVertices(), _cube.getIndices());
        Core::Material cubeMat{};
        cubeMat.metallic  = 1.0f;
        cubeMat.roughness = 0.01f;
        cubeMat.textureId = -1;
        Core::GetAssetManager().addMaterial("loadingCubeMat", cubeMat);

        // ECS modules
        world.import<flecs::stats>();
        world.set<flecs::Rest>({});
        world.import<Module::Persistent>();
        world.import<Module::Hidden>();
        world.import<Module::Transform>();
        world.import<Module::MeshTransform>();
        world.import<Module::TransformID>();
        world.import<Module::Mesh>();
        world.import<Module::SkeletonMesh>();
        world.import<Module::AnimationStateMachine>();
        world.import<Module::Light>();
        world.import<Module::FlyingCamera>();
        world.import<Module::CharacterCamera>();
        world.import<Module::PhysicsModule>();
        world.import<Module::PhysicsBodyModule>();
        world.import<Module::BoxColliderModule>();
        world.import<Module::RigidBodyModule>();
        world.import<Module::CharacterCapsuleModule>();

        // Load the city level (shows loading screen, then sets up
        // the 10x10 streaming zone grid and proximity system)
        auto cityLevel = std::make_shared<CityLevel>();
        loadLevel(cityLevel);
    }

    void onLoadingScreenSetup() override {
        using namespace Lca;
        world.entity("LoadingScreenCamera")
            .set<Component::FlyingCamera>({
                .position      = glm::vec3(0.0f, 0.0f, 5.0f),
                .angle         = Component::FlyingCamera::lookAt(glm::vec3(0.0f,0.0f,5.0f), glm::vec3(0.0f)),
                .speed         = 10.0f,
                .rotationSpeed = 0.005f,
                .fov           = glm::radians(60.0f),
                .nearClip      = 0.1f,
                .farClip       = 1000.0f
            })
            .add<Component::Transform>();

        auto cube = world.entity("LoadingScreenCube");
        cube.set(Component::Transform{glm::vec3(0,0,0), 0.0f, glm::vec3(0,1,0), glm::vec3(1.0f)});
        cube.add<Component::TransformID>();
        auto cubeMesh = world.entity("LoadingScreenCubeMesh");
        cubeMesh.add(flecs::ChildOf, cube);
        cubeMesh.set<Component::Mesh>({
            Core::GetAssetManager().getMeshId("cube"),
            Core::GetAssetManager().getMaterialId("loadingCubeMat"),
            Core::GetRenderer().getMeshPipelineId("basic")
        });

        world.entity("LoadingSun")
            .set<Component::DirectionalLight>({
                .color     = glm::vec3(1.0f, 0.98f, 0.9f),
                .intensity = 2.0f,
            });

        world.system<Component::Transform>("Rotate Loading Cube")
            .without<Component::Hidden>()
            .each([](flecs::entity, Component::Transform& t) {
                glm::quat dr = glm::angleAxis(Time::deltaTime, glm::vec3(0.0f, 1.0f, 0.0f));
                t.rotation = dr * t.rotation;
            });
    }

    void onLevelReady() override {
        std::cout << "[Example6] Level loaded. WASD to move, Shift to run, Space to jump, Left-click to attack. Proximity streaming active.\n";
    }

    void onStreamingLevelLoaded(const std::string& levelId) override {
        std::cout << "[Example6] Streamed IN:  " << levelId << "\n";
    }

    void onStreamingLevelUnloaded(const std::string& levelId) override {
        std::cout << "[Example6] Streamed OUT: " << levelId << "\n";
    }

    void onUpdate() override {
        using namespace Lca;
        // Manual override: 0 = stream out ALL
        if (Input::key0.pressed) streamOutAll();
    }
};
