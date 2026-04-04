#pragma once

#include "Level.hpp"
#include "AssetManager.hpp"
#include "Renderer.hpp"
#include "Mesh.hpp"
#include "MeshTransform.hpp"
#include "SkeletonMesh.hpp"
#include "FlyingCamera.hpp"
#include "CharacterCamera.hpp"
#include "AnimationStateMachine.hpp"
#include "Light.hpp"
#include "Time.hpp"
#include "Input.hpp"
#include "CharacterCapsule.hpp"
#include "RigidBody.hpp"
#include "BoxCollider.hpp"
#include "Velocity.hpp"
#include "Shape.hpp"

#include <string>
#include <vector>
#include <iostream>
#include <cmath>

using namespace Lca;

// ──────────────────────────────────────────────────────────────
// Procedural sky texture generator
// ──────────────────────────────────────────────────────────────
namespace ProceduralTex {

    inline float hash(float x, float y) {
        float v = std::sin(x * 127.1f + y * 311.7f) * 43758.5453f;
        return v - std::floor(v);
    }

    inline float noise(float x, float y) {
        float ix = std::floor(x), iy = std::floor(y);
        float fx = x - ix, fy = y - iy;
        float a = hash(ix, iy);
        float b = hash(ix + 1.0f, iy);
        float c = hash(ix, iy + 1.0f);
        float d = hash(ix + 1.0f, iy + 1.0f);
        float ux = fx * fx * (3.0f - 2.0f * fx);
        float uy = fy * fy * (3.0f - 2.0f * fy);
        return a + (b - a) * ux + (c - a) * uy + (a - b - c + d) * ux * uy;
    }

    inline float fbm(float x, float y, int octaves = 4) {
        float val = 0.0f, amp = 0.5f;
        for (int i = 0; i < octaves; ++i) {
            val += amp * noise(x, y);
            x *= 2.0f;
            y *= 2.0f;
            amp *= 0.5f;
        }
        return val;
    }

    inline uint8_t clamp8(float v) {
        int i = static_cast<int>(v * 255.0f);
        return static_cast<uint8_t>(i < 0 ? 0 : (i > 255 ? 255 : i));
    }

    // Generate a procedural sky RGBA texture.
    // v=0 → zenith (deep blue), v≈0.5 → horizon (pale blue + clouds), v=1 → nadir.
    inline std::vector<uint32_t> generateSky(int w, int h, float seed) {
        std::vector<uint32_t> pixels(w * h);
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                float u = static_cast<float>(x) / w;
                float v = static_cast<float>(y) / h;

                float n = fbm(u * 6.0f + seed, v * 4.0f + seed * 0.5f, 5);

                // Sky gradient: deep blue at zenith (v=0), pale blue at horizon (v≈0.5)
                float t = std::fmin(1.0f, v * 2.0f);
                float r = 0.15f + 0.45f * t;
                float g = 0.30f + 0.45f * t;
                float b = 0.80f + 0.15f * t;

                // Cloud layer concentrated around the horizon band
                float cloudBand = 1.0f - std::fabs(v - 0.45f) * 4.0f;
                cloudBand = std::fmax(0.0f, cloudBand);
                float cloud = std::fmax(0.0f, n - 0.45f) * 3.0f * cloudBand;
                cloud = std::fmin(1.0f, cloud);

                r += cloud * (1.0f - r);
                g += cloud * (1.0f - g);
                b += cloud * (1.0f - b);

                r = std::fmax(0.0f, std::fmin(1.0f, r));
                g = std::fmax(0.0f, std::fmin(1.0f, g));
                b = std::fmax(0.0f, std::fmin(1.0f, b));

                uint32_t px = (255u << 24)
                    | (static_cast<uint32_t>(clamp8(b)) << 16)
                    | (static_cast<uint32_t>(clamp8(g)) << 8)
                    | (static_cast<uint32_t>(clamp8(r)));
                pixels[y * w + x] = px;
            }
        }
        return pixels;
    }

} // namespace ProceduralTex

// ──────────────────────────────────────────────────────────────
// CharacterController — reads input, drives the Jolt
// CharacterCapsule and the AnimationStateMachine blend param.
// ──────────────────────────────────────────────────────────────

struct CharacterController {
    bool  jumpHeld     = false;

    void update(Component::CharacterCapsule& capsule,
                Component::AnimationStateMachine& stateMachine)
    {
        if (!capsule.character) return;

        // ── Turning ───────────────────────────────────────────
        if (Input::keyA.down) capsule.turn(Time::deltaTime);
        if (Input::keyD.down) capsule.turn(-Time::deltaTime);

        // ── Forward / backward speed ──────────────────────────
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

        // ── Build desired horizontal movement from capsule facing ──
        JPH::Vec3 fwdJolt = capsule.rotation * JPH::Vec3(0.0f, 0.0f, 1.0f);
        glm::vec3 moveDir(fwdJolt.GetX(), 0.0f, fwdJolt.GetZ());
        if (glm::length(moveDir) > 0.0f) {
            moveDir = glm::normalize(moveDir);
        }
        capsule.move(moveDir);

        // ── Jump ──────────────────────────────────────────────
        const bool jumpDown = Input::keySpace.down;
        if (jumpDown && !jumpHeld) {
            capsule.jump();
        }
        jumpHeld = jumpDown;

        // ── Drive blend space parameter ───────────────────────
        if (auto* blendSpace = std::get_if<Component::BlendSpace1D>(
                &stateMachine.states["IdleWalkRun"])) {
            blendSpace->blendParameter = capsule.moveSpeed / capsule.maxRunSpeed;
        }

        // ── Attack on left mouse button ───────────────────────
        if (Input::buttonLeft.down && stateMachine.currentStateName != "Attack") {
            stateMachine.setCurrentState("Attack");
        }
    }
};

// ──────────────────────────────────────────────────────────────
// WizardLevel — loads the Wizard skeleton model, sets up
// animations, a physics floor, and a CharacterCapsule player.
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

        // ── Skybox (inverted sphere + procedural sky texture) ──
        Shape::Sphere skySphere(1, 32, 32, glm::vec3(0.0f), glm::vec4(1.0f), true);
        am.addMesh("skyboxMesh", skySphere.getVertices(), skySphere.getIndices());

        constexpr int kSkyTexW = 1280;
        constexpr int kSkyTexH = 1280;
        auto skyPixels = ProceduralTex::generateSky(kSkyTexW, kSkyTexH, 400.0f);
        Core::Texture skyTex = Core::createTexture(kSkyTexW, kSkyTexH, skyPixels.data());
        am.addTexture("skyboxTex", skyTex);

        Core::Material skyMat{};
        skyMat.textureId = static_cast<int32_t>(am.getTextureId("skyboxTex"));
        skyMat.roughness = 1.0f;
        skyMat.metallic  = 0.0f;
        am.addMaterial("skyboxMat", skyMat);
    }

    void setupScene(flecs::world& world) override {
        auto& am  = Core::GetAssetManager();
        auto& ren = Core::GetRenderer();

        uint32_t pipelineId     = ren.getSkeletonMeshPipelineId("skeletonPBR");
        uint32_t meshPipelineId = ren.getMeshPipelineId("basic");
        constexpr float playerCapsuleHeight = 1.0f;
        constexpr float playerCapsuleRadius = 0.3f;

        // ── Skybox ────────────────────────────────────────────
        {
            auto e = world.entity("Skybox");
            e.set(Component::Transform{
                glm::vec3(0.0f),
                glm::radians(-90.0f), glm::vec3(1,0,0),
                glm::vec3(400.0f)});
            e.add<Component::TransformID>();
            e.add<Component::Static>();

            auto m = world.entity("SkyboxMesh");
            m.add(flecs::ChildOf, e);
            m.set<Component::Mesh>({
                am.getMeshId("skyboxMesh"),
                am.getMaterialId("skyboxMat"),
                meshPipelineId
            });
        }

        // ── Static rigid-body floor (large cube) ──────────────
        {
            glm::vec3 floorHalfExtent(25.0f, 0.5f, 25.0f);
            auto floor = world.entity("Floor");
            floor.set(Component::Transform{glm::vec3(0.0f, -0.5f, 0.0f), 0.0f, glm::vec3(0,1,0), glm::vec3(floorHalfExtent)});
            floor.add<Component::TransformID>();
            floor.set<Component::BoxCollider>({ floorHalfExtent });
            floor.set<Component::StaticRigidBody>({ .objectLayer = Layers::STATIC_ENVIRONMENT, .friction = 0.8f, .restitution = 0.0f });

            auto floorMesh = world.entity("FloorMesh");
            floorMesh.add(flecs::ChildOf, floor);
            floorMesh.set<Component::Mesh>({
                am.getMeshId("floorCube"),
                am.getMaterialId("floorMat"),
                meshPipelineId
            });
        }

        // ── Jumpable platform cubes (static rigid bodies) ─────
        struct PlatformDef { glm::vec3 pos; glm::vec3 halfExtent; };
        PlatformDef platforms[] = {
            { glm::vec3( 4.0f, 0.5f,  0.0f), glm::vec3(1.5f, 0.5f, 1.5f) },
            { glm::vec3( 8.0f, 1.5f,  0.0f), glm::vec3(1.5f, 0.5f, 1.5f) },
            { glm::vec3(12.0f, 2.5f,  2.0f), glm::vec3(1.5f, 0.5f, 1.5f) },
            { glm::vec3( 0.0f, 1.0f, -5.0f), glm::vec3(2.0f, 0.5f, 2.0f) },
            { glm::vec3(-5.0f, 0.75f, 3.0f), glm::vec3(1.0f, 0.75f, 1.0f) },
        };
        for (int i = 0; i < 5; ++i) {
            auto& p = platforms[i];
            auto plat = world.entity(("Platform_" + std::to_string(i)).c_str());
            plat.set(Component::Transform{p.pos, 0.0f, glm::vec3(0,1,0), p.halfExtent});
            plat.add<Component::TransformID>();
            plat.set<Component::BoxCollider>({ p.halfExtent });
            plat.set<Component::StaticRigidBody>({ .objectLayer = Layers::STATIC_ENVIRONMENT, .friction = 0.8f, .restitution = 0.0f });

            auto platMesh = world.entity(("PlatformMesh_" + std::to_string(i)).c_str());
            platMesh.add(flecs::ChildOf, plat);
            platMesh.set<Component::Mesh>({
                am.getMeshId("platformCube"),
                am.getMaterialId("platformMat"),
                meshPipelineId
            });
        }

        // ── Wizard player entity ──────────────────────────────
        auto player = world.entity("Player");
        const float floorTopY = 0.0f;
        const float playerCenterY = floorTopY + (playerCapsuleHeight * 0.5f) + playerCapsuleRadius;
        const float meshLocalYOffset = -(playerCapsuleHeight * 0.5f + playerCapsuleRadius);
        player.set(Component::Transform{glm::vec3(0.0f, playerCenterY, 0.0f), 0.0f, glm::vec3(0,1,0), glm::vec3(1.0f)});
        player.set(Component::MeshTransform{Component::Transform{glm::vec3(0.0f, meshLocalYOffset, 0.0f), 0.0f, glm::vec3(0,1,0), glm::vec3(1.0f)}});
        player.add<Component::TransformID>();
        player.add<Component::SkeletonInstanceID>();

        // Skeleton mesh children (one per sub-mesh in the model)
        for (size_t k = 0; k < model.size(); ++k) {
            const auto& meshInst = model[k];
            auto child = world.entity(("PlayerMesh_" + std::to_string(k)).c_str());
            child.add(flecs::ChildOf, player);
            child.set<Component::SkeletonMesh>({
                .skeletonName = "Wizard",
                .meshID       = meshInst.meshId,
                .materialID   = meshInst.materialId,
                .pipelineID   = pipelineId,
                .nodeIndex    = meshInst.nodeIndex
            });
        }

        // ── Physics CharacterCapsule ──────────────────────────
        player.set<Component::CharacterCapsule>({
            .capsuleHeight = playerCapsuleHeight,
            .capsuleRadius = playerCapsuleRadius,
            .layer         = Layers::PLAYER_BODY,
            .speedDecrement = 4.0f
        });

        // ── Animation State Machine ───────────────────────────
        auto animKey = [](const std::string& rawName) -> std::string {
            return "Wizard:" + rawName;
        };

        std::string idleName, walkName, runName, spell2Name, deathName;
        for (const auto& n : animNames) {
            std::string lower = n;
            std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
            if (lower.find("idle")   != std::string::npos) idleName   = n;
            if (lower.find("walk")   != std::string::npos) walkName   = n;
            if (lower.find("run")    != std::string::npos) runName    = n;
            if (lower.find("spell2") != std::string::npos) spell2Name = n;
            if (lower.find("death")  != std::string::npos || lower.find("die") != std::string::npos) deathName = n;
        }

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
            blendSpace.blendParameter     = 0.0f;
            blendSpace.animationSpeed[0]  = 1.0f;
            blendSpace.animationSpeed[1]  = 1.0f;
            blendSpace.animationSpeed[2]  = 1.0f;

            asm_.addState("IdleWalkRun", blendSpace);
            asm_.setCurrentState("IdleWalkRun");
        } else if (!idleName.empty()) {
            Component::SingleAnimation idle;
            idle.animationName  = animKey(idleName);
            idle.animationSpeed = 1.0f;
            idle.looping        = true;
            asm_.addState("IdleWalkRun", idle);
            asm_.setCurrentState("IdleWalkRun");
        }

        // --- Attack (upper-body action blend) ---
        if (!spell2Name.empty()) {
            Component::SingleAnimation spell;
            spell.animationName  = animKey(spell2Name);
            spell.animationSpeed = 1.0f;
            spell.looping        = false;
            asm_.addState("Spell2", spell);

            const auto& skelData = am.getSkeletonData("Wizard");
            std::vector<uint32_t> upperBodyMask;
            const std::vector<std::string> upperBoneNames = {
                "Torso", "Neck", "Head",
                "Shoulder.L", "UpperArm.L", "LowerArm.L", "Fist.L",
                "Shoulder.R", "UpperArm.R", "LowerArm.R", "Fist.R",
                "mixamorig:Spine1", "mixamorig:Spine2",
                "mixamorig:Neck", "mixamorig:Head",
                "mixamorig:LeftShoulder",  "mixamorig:LeftArm",  "mixamorig:LeftForeArm",  "mixamorig:LeftHand",
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
                attack.actionState       = "Spell2";
                attack.baseState         = "IdleWalkRun";
                attack.nodeMask          = upperBodyMask;
                attack.blendInDuration   = 0.25f;
                attack.blendOutDuration  = 0.25f;
                asm_.addState("Attack", attack);
            }
        }

        // --- Death ---
        if (!deathName.empty()) {
            Component::SingleAnimation death;
            death.animationName  = animKey(deathName);
            death.animationSpeed = 1.0f;
            death.looping        = false;
            asm_.addState("Death", death);
        }

        player.set(asm_);
        player.set<CharacterController>({});

        // ── Character control system ──────────────────────────
        world.system<CharacterController, Component::CharacterCapsule, Component::AnimationStateMachine>(
            "Character Controller Update")
            .kind(flecs::PreUpdate)
            .each([](CharacterController& ctrl, Component::CharacterCapsule& capsule,
                     Component::AnimationStateMachine& asm_) {
                ctrl.update(capsule, asm_);
            });

        // ── Camera following the player ───────────────────────
        world.entity("Camera")
            .set<Component::CharacterCamera>({
                .target        = player,
                .yaw           = glm::pi<float>(),
                .pitch         = 0.15f,
                .distance      = 8.0f,
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
    }

private:
    Core::Model model;
    std::vector<std::string> animNames;
};
