#pragma once

#include "Level.hpp"
#include "AssetManager.hpp"
#include "Renderer.hpp"
#include "Mesh.hpp"
#include "SkeletonMesh.hpp"
#include "FlyingCamera.hpp"
#include "CharacterCamera.hpp"
#include "AnimationStateMachine.hpp"
#include "Light.hpp"
#include "Time.hpp"
#include "Input.hpp"

#include <string>
#include <vector>
#include <iostream>
#include <cmath>

using namespace Lca;

struct MultiThreaded1{
    uint32_t buffer;
};

struct MultiThreaded2{
    uint32_t buffer;
};

struct MultiThreaded3{
    uint32_t buffer;
};

struct MultiThreaded4{
    uint32_t buffer;
};

struct MultiThreaded5{
    uint32_t buffer;
};
// ──────────────────────────────────────────────────────────────
// Primitive CharacterCapsule — no physics engine, just moves
// the owning Transform directly via keyboard input.
// ──────────────────────────────────────────────────────────────

struct CharacterCapsule {
    float moveSpeed      = 0.0f;
    float maxWalkSpeed   = 2.0f;
    float maxRunSpeed    = 5.0f;
    float acceleration   = 6.0f;
    float deceleration   = 8.0f;
    float turnSpeed      = 3.0f;
    bool  isRunning      = false;

    void incrSpeed() {
        float limit = isRunning ? maxRunSpeed : maxWalkSpeed;
        moveSpeed = std::min(moveSpeed + acceleration * Time::deltaTime, limit);
    }

    void decrSpeed() {
        moveSpeed = std::max(moveSpeed - deceleration * Time::deltaTime, 0.0f);
    }

    void turn(float sign) {
        yaw += sign * turnSpeed * Time::deltaTime;
    }

    void move(Component::Transform& transform) {
        // Forward direction from yaw
        glm::vec3 forward(std::sin(yaw), 0.0f, std::cos(yaw));

        transform.position += forward * moveSpeed * Time::deltaTime;
        transform.rotation = glm::angleAxis(yaw, glm::vec3(0.0f, 1.0f, 0.0f));
    }

    float yaw = 0.0f;
};

// ──────────────────────────────────────────────────────────────
// CharacterController — reads input, drives CharacterCapsule
// and the AnimationStateMachine blend parameter.
// ──────────────────────────────────────────────────────────────

struct CharacterController {
    void update(CharacterCapsule& capsule,
                Component::AnimationStateMachine& stateMachine,
                Component::Transform& transform)
    {
        // Turning
        if (Input::keyA.down) capsule.turn(Time::deltaTime > 0 ? 1.0f : 0.0f);
        if (Input::keyD.down) capsule.turn(Time::deltaTime > 0 ? -1.0f : 0.0f);

        // Movement
        if (Input::keyW.down) {
            capsule.isRunning = Input::keyShift.down;
            if (!capsule.isRunning && capsule.moveSpeed > capsule.maxWalkSpeed)
                capsule.decrSpeed();
            else
                capsule.incrSpeed();
        } else {
            capsule.isRunning = false;
            capsule.decrSpeed();
        }

        capsule.move(transform);

        // Drive blend space parameter: speed / maxRunSpeed → [0,1]
        if (auto* blendSpace = std::get_if<Component::BlendSpace1D>(
                &stateMachine.states["IdleWalkRun"])) {
            blendSpace->blendParameter = capsule.moveSpeed / capsule.maxRunSpeed;
        }

        // Attack on left mouse button
        if (Input::buttonLeft.down && stateMachine.currentStateName != "Attack") {
            stateMachine.setCurrentState("Attack");
        }
    }
};

// ──────────────────────────────────────────────────────────────
// WizardLevel — loads the Wizard skeleton model, sets up
// a BlendSpace1D (Idle/Walk/Run) and an upper-body attack.
// ──────────────────────────────────────────────────────────────

class WizardLevel : public Level {
public:
    void loadAssets() override {
        auto& am = Core::GetAssetManager();

        const std::string gltfPath = "C:/Users/robry/Desktop/3DModels/Wizard.gltf";

        // Skeleton + meshes + materials + textures
        model = am.loadSkeletonModel("Wizard", gltfPath);

        // Animations (all clips in the file)
        animNames = am.loadAnimations("Wizard", gltfPath);

        // Bind every animation to the skeleton node indices
        for (const auto& animName : animNames) {
            am.extractAnimationForSkeleton(animName, "Wizard");
        }
    }

    void setupScene(flecs::world& world) override {
        auto& am  = Core::GetAssetManager();
        auto& ren = Core::GetRenderer();

        // ── Pipeline ──────────────────────────────────────────
        uint32_t pipelineId = ren.getSkeletonMeshPipelineId("skeletonPBR");

        uint32_t width = 3, height = 3;
        uint32_t wizardsWidth = 31, wizardsHeight = 30;
        std::vector<flecs::entity> wizards(wizardsWidth * wizardsHeight);
        for(uint32_t i = 0; i < wizardsWidth; ++i){
            for(uint32_t j = 0; j < wizardsHeight; ++j){
                auto& e = wizards[i * wizardsHeight + j];
                e = world.entity();
                e.set(Component::Transform{glm::vec3(i * width, 0.0f, j * height), 0.0f, glm::vec3(0,1,0), glm::vec3(1.0f)});
                e.add<Component::TransformID>();
                e.add<Component::SkeletonInstanceID>();

                // switch(j % 5) {
                //     case 0: e.set(MultiThreaded1{}); break;
                //     case 1: e.set(MultiThreaded2{}); break;
                //     case 2: e.set(MultiThreaded3{}); break;
                //     case 3: e.set(MultiThreaded4{}); break;
                //     case 4: e.set(MultiThreaded5{}); break;
                // }

                // ── Skeleton mesh children (one per sub-mesh in the model) ──
                for (size_t k = 0; k < model.size(); ++k) {
                    const auto& meshInst = model[k];
                    auto child = world.entity((std::string("WizardMesh_") + std::to_string(i) + "_" + std::to_string(j) + "_" + std::to_string(k)).c_str());
                    child.add(flecs::ChildOf, e);
                    child.set<Component::SkeletonMesh>({
                        .skeletonName = "Wizard",
                        .meshID       = meshInst.meshId,
                        .materialID   = meshInst.materialId,
                        .pipelineID   = pipelineId,
                        .nodeIndex    = meshInst.nodeIndex
                    });
                }
            }
        }
        
        // ── Camera ────────────────────────────────────────────
        world.entity("Camera")
            .set<Component::CharacterCamera>({
                .target        = wizards[wizards.size()/2], // Look at center wizard
                .yaw           = glm::pi<float>(),
                .pitch         = 0.15f,
                .distance      = 16.0f,
                .rotationSpeed = 0.005f,
                .fov           = glm::radians(60.0f),
                .nearClip      = 0.1f,
                .farClip       = 500.0f,
                .targetOffset  = glm::vec3(0.0f, 1.5f, 0.0f)
            })
            .add<Component::Transform>();

        // ── Sun light ─────────────────────────────────────────
        world.entity("Sun")
            .set<Component::DirectionalLight>({
                .color     = glm::vec3(1.0f, 0.98f, 0.9f),
                .intensity = 2.0f,
            });

        // ── Debug cube (test regular mesh rendering) ──────────
        {
            uint32_t meshPipeId = ren.getMeshPipelineId("basic");
            auto cube = world.entity("DebugCube");
            cube.set(Component::Transform{glm::vec3(3.0f, 1.0f, 0.0f), 0.0f, glm::vec3(0,1,0), glm::vec3(1.0f)});
            cube.add<Component::TransformID>();
            auto cubeMesh = world.entity("DebugCubeMesh");
            cubeMesh.add(flecs::ChildOf, cube);
            cubeMesh.set<Component::Mesh>({
                am.getMeshId("debugCube"),
                am.getMaterialId("debugCubeMat"),
                meshPipeId
            });
        }

        // Build animation state names (need extractAnimationForSkeleton key format)
        auto animKey = [](const std::string& rawName) -> std::string {
            return "Wizard:" + rawName;
        };

        // Find which raw animation names correspond to Idle/Walk/Run
        // The gltf animations come in as "Wizard/<clip-name>"
        std::string idleName, walkName, runName, spell2Name, deathName;
        for (const auto& n : animNames) {
            std::string lower = n;
            std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
            if (lower.find("idle") != std::string::npos)  idleName = n;
            if (lower.find("walk") != std::string::npos)  walkName = n;
            if (lower.find("run")  != std::string::npos)  runName  = n;
            if (lower.find("spell2") != std::string::npos ) spell2Name = n;
            if (lower.find("death") != std::string::npos || lower.find("die") != std::string::npos)    deathName  = n;
        }

        // Set up the AnimationStateMachine
        Component::AnimationStateMachine asm_;
        asm_.skeletonName = "Wizard";

        // --- IdleWalkRun BlendSpace1D ---
        if (!idleName.empty() && !walkName.empty() && !runName.empty()) {
            Component::BlendSpace1D blendSpace;
            blendSpace.pointCount = 3;
            blendSpace.animationNames[0] = animKey(idleName);
            blendSpace.animationNames[1] = animKey(walkName);
            blendSpace.animationNames[2] = animKey(runName);
            blendSpace.animationPoints[0] = 0.0f;
            blendSpace.animationPoints[1] = 0.5f;
            blendSpace.animationPoints[2] = 1.0f;
            blendSpace.blendParameter = 0.0f;
            blendSpace.animationSpeed[0] = 1.0f;
            blendSpace.animationSpeed[1] = 1.0f;
            blendSpace.animationSpeed[2] = 1.0f;

            asm_.addState("IdleWalkRun", blendSpace);
            asm_.setCurrentState("IdleWalkRun");
        } else if (!idleName.empty()) {
            // Fallback: just play idle
            Component::SingleAnimation idle;
            idle.animationName = animKey(idleName);
            idle.animationSpeed = 1.0f;
            idle.looping = true;
            asm_.addState("IdleWalkRun", idle);
            asm_.setCurrentState("IdleWalkRun");
        }

        // --- Attack (upper-body action blend) ---
        if (!spell2Name.empty()) {
            Component::SingleAnimation spell;
            spell.animationName = animKey(spell2Name);
            spell.animationSpeed = 1.0f;
            spell.looping = false;
            asm_.addState("Spell2", spell);

            // Build upper-body mask from skeleton node names
            const auto& skelData = am.getSkeletonData("Wizard");
            std::vector<uint32_t> upperBodyMask;
            const std::vector<std::string> upperBoneNames = {
                "Torso", "Neck", "Head",
                "Shoulder.L", "UpperArm.L", "LowerArm.L", "Fist.L",
                "Shoulder.R", "UpperArm.R", "LowerArm.R", "Fist.R",
                // Common alternative naming conventions
                "mixamorig:Spine1", "mixamorig:Spine2",
                "mixamorig:Neck", "mixamorig:Head",
                "mixamorig:LeftShoulder", "mixamorig:LeftArm", "mixamorig:LeftForeArm", "mixamorig:LeftHand",
                "mixamorig:RightShoulder", "mixamorig:RightArm", "mixamorig:RightForeArm", "mixamorig:RightHand"
            };
            for (const auto& boneName : upperBoneNames) {
                auto it = skelData.nodeNameToIndex.find(boneName);
                if (it != skelData.nodeNameToIndex.end()) {
                    upperBodyMask.push_back(static_cast<uint32_t>(it->second));
                }
            }

            if (!upperBodyMask.empty()) {
                Component::ActionAnimationBlend attack;
                attack.actionState = "Spell2";
                attack.baseState   = "IdleWalkRun";
                attack.nodeMask    = upperBodyMask;
                attack.blendInDuration  = 0.25f;
                attack.blendOutDuration = 0.25f;
                asm_.addState("Attack", attack);
            }
        }

        // --- Death ---
        if (!deathName.empty()) {
            Component::SingleAnimation death;
            death.animationName = animKey(deathName);
            death.animationSpeed = 1.0f;
            death.looping = false;
            asm_.addState("Death", death);
        }

        for(auto& wizard : wizards) {
            wizard.set(asm_);
            wizard.set<CharacterCapsule>({});
            wizard.set<CharacterController>({});
        }

        // ── Character control system ──────────────────────────
        world.system<CharacterController, CharacterCapsule, Component::AnimationStateMachine, Component::Transform>(
            "Character Controller Update")
            .each([](CharacterController ctrl, CharacterCapsule& capsule,
                     Component::AnimationStateMachine& asm_, Component::Transform& transform) {
                ctrl.update(capsule, asm_, transform);
            });
    }

private:
    Core::Model model;
    std::vector<std::string> animNames;
};
