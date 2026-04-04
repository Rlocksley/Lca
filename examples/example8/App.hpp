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
// Example 8 — Skeleton Mesh with Physics CharacterCapsule
//
// Same Wizard model + animations as Example 7, but driven
// by Jolt CharacterVirtual (CharacterCapsule) instead of a
// hand-rolled capsule.  A static rigid-body cube floor
// provides collision.  The player can jump with Space.
// ──────────────────────────────────────────────────────────────

class ExampleApp : public Lca::Application {
public:
    ExampleApp(const std::string& title, int width, int height)
        : Application(title, width, height) {}

protected:

    void onInit() override {
        using namespace Lca;

        // ── Regular PBR pipeline (for floor cube) ─────────────
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

        // ── Floor cube mesh + material ────────────────────────
        Shape::Cube floorCube(glm::vec3(1.0f), glm::vec3(0.0f), glm::vec4(0.4f, 0.4f, 0.4f, 1.0f));
        Core::GetAssetManager().addMesh("floorCube", floorCube.getVertices(), floorCube.getIndices());
        Core::Material floorMat{};
        floorMat.metallic  = 0.0f;
        floorMat.roughness = 0.8f;
        floorMat.textureId = -1;
        Core::GetAssetManager().addMaterial("floorMat", floorMat);

        // ── Platform cube mesh + material ─────────────────────
        Shape::Cube platformCube(glm::vec3(1.0f), glm::vec3(0.0f), glm::vec4(0.8f, 0.2f, 0.2f, 1.0f));
        Core::GetAssetManager().addMesh("platformCube", platformCube.getVertices(), platformCube.getIndices());
        Core::Material platMat{};
        platMat.metallic  = 0.1f;
        platMat.roughness = 0.6f;
        platMat.textureId = -1;
        Core::GetAssetManager().addMaterial("platformMat", platMat);

        // ── ECS modules ───────────────────────────────────────
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

        // ── Load the wizard level ─────────────────────────────
        auto wizardLevel = std::make_shared<WizardLevel>();
        loadLevel(wizardLevel);
    }

    void onLoadingScreenSetup() override {
        using namespace Lca;
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

        // Visible cube so the loading screen isn't empty
        uint32_t meshPipeId = Core::GetRenderer().getMeshPipelineId("basic");
        auto cube = world.entity("LoadingCube");
        cube.set(Component::Transform{glm::vec3(0.0f, 0.0f, 0.0f), 0.0f, glm::vec3(0,1,0), glm::vec3(1.0f)});
        cube.add<Component::TransformID>();
        auto cubeMesh = world.entity("LoadingCubeMesh");
        cubeMesh.add(flecs::ChildOf, cube);
        cubeMesh.set<Component::Mesh>({
            Core::GetAssetManager().getMeshId("floorCube"),
            Core::GetAssetManager().getMaterialId("floorMat"),
            meshPipeId
        });

        world.entity("LoadingSun")
            .set<Component::DirectionalLight>({
                .color     = glm::vec3(1.0f, 0.98f, 0.9f),
                .intensity = 2.0f,
            });
    }

    void onLevelReady() override {
        std::cout << "[Example8] Wizard loaded. WASD to move, Shift to run, Space to jump, Left-click to attack.\n";
    }
};
