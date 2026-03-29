#pragma once

#include "Application.hpp"
#include "Light.hpp"
#include "Shape.hpp"
#include "CharacterCamera.hpp"
#include "Lev.hpp"
#include "Input.hpp"
#include <iostream>

// ──────────────────────────────────────────────────────────────
// Example 7 — Skeleton Mesh with Character Controller
//
// Loads a Wizard glTF model with skeleton, meshes, and
// animations.  WASD moves the character, Shift to run,
// Left-click for an upper-body attack.
// A FlyingCamera orbits freely with right-click + mouse.
// ──────────────────────────────────────────────────────────────

class ExampleApp : public Lca::Application {
public:
    ExampleApp(const std::string& title, int width, int height)
        : Application(title, width, height) {}

protected:

    void onInit() override {
        using namespace Lca;

        // ── Regular PBR pipeline (for debug cube) ─────────────
        Core::GraphicsPipelineConfig meshPipelineConfig{};
        meshPipelineConfig.vertexShader   = "shader/mesh_pbr.vert.spv";
        meshPipelineConfig.fragmentShader = "shader/mesh_pbr.frag.spv";
        Core::MeshPipeline meshPipeline(meshPipelineConfig);
        Core::GetRenderer().addMeshPipeline("basic", std::move(meshPipeline), 4096);

        // ── Skeleton PBR pipeline ─────────────────────────────
        Core::GraphicsPipelineConfig pipelineConfig{};
        pipelineConfig.vertexShader   = "shader/skeleton_mesh_pbr.vert.spv";
        pipelineConfig.fragmentShader = "shader/mesh_pbr.frag.spv";
        Core::SkeletonMeshPipeline skelPipeline(pipelineConfig);
        Core::GetRenderer().addSkeletonMeshPipeline("skeletonPBR", std::move(skelPipeline), 2*4096);

        // ── Debug cube mesh + material ────────────────────────
        Shape::Cube debugCube(glm::vec3(1.0f), glm::vec3(0.0f), glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));
        Core::GetAssetManager().addMesh("debugCube", debugCube.getVertices(), debugCube.getIndices());
        Core::Material cubeMat{};
        cubeMat.metallic  = 0.0f;
        cubeMat.roughness = 0.5f;
        cubeMat.textureId = -1;
        Core::GetAssetManager().addMaterial("debugCubeMat", cubeMat);

        // ── ECS modules ───────────────────────────────────────
        world.import<flecs::stats>();
        world.set<flecs::Rest>({});
        world.import<Module::Persistent>();
        world.import<Module::Hidden>();
        world.import<Module::Transform>();
        world.import<Module::TransformID>();
        world.import<Module::Mesh>();
        world.import<Module::SkeletonMesh>();
        world.import<Module::AnimationStateMachine>();
        world.import<Module::Light>();
        world.import<Module::FlyingCamera>();
        world.import<Module::CharacterCamera>();

        // ── Load the wizard level ─────────────────────────────
        auto wizardLevel = std::make_shared<WizardLevel>();
        loadLevel(wizardLevel);
    }

    void onLoadingScreenSetup() override {
        using namespace Lca;
        // Minimal loading screen — just a camera
        world.entity("LoadingCamera")
            .set<Component::FlyingCamera>({
                .position      = glm::vec3(0.0f, 2.0f, 5.0f),
                .angle         = Component::FlyingCamera::lookAt(
                                     glm::vec3(0.0f, 2.0f, 5.0f),
                                     glm::vec3(0.0f)),
                .speed         = 10.0f,
                .rotationSpeed = 0.005f,
                .fov           = glm::radians(60.0f),
                .nearClip      = 0.1f,
                .farClip       = 500.0f
            })
            .add<Component::Transform>();
    }

    void onLevelReady() override {
        std::cout << "[Example7] Wizard loaded. WASD to move, Shift to run, Left-click to attack.\n";
    }
};
