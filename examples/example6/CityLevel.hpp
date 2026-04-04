#pragma once

#include "Level.hpp"
#include "AssetManager.hpp"
#include "Renderer.hpp"
#include "Helpers.hpp"
#include "Mesh.hpp"
#include "MeshTransform.hpp"
#include "SkeletonMesh.hpp"
#include "FlyingCamera.hpp"
#include "CharacterCamera.hpp"
#include "AnimationStateMachine.hpp"
#include "Light.hpp"
#include "Time.hpp"
#include "Input.hpp"
#include "Vertex.hpp"
#include "CharacterCapsule.hpp"
#include "RigidBody.hpp"
#include "BoxCollider.hpp"
#include "Velocity.hpp"
#include "Shape.hpp"
#include "ZoneRng.hpp"
#include "HardcodedMeshes6.hpp"
#include "ProceduralTex.hpp"
#include "Controllers.hpp"
#include "ZoneGrid.hpp"
#include "ZoneStreamingLevel.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

using namespace Lca;

class CityLevel : public Lca::Level {
public:
    void loadAssets() override {
        using namespace Lca;

        auto& am = Core::GetAssetManager();

        // Shared materials (used by every streaming zone)
        Core::Material carMat{};
        carMat.textureId = -1;
        carMat.roughness = 0.3f;
        carMat.metallic  = 0.7f;
        am.addMaterial("car_mat", carMat);

        Core::Material lanternMat{};
        lanternMat.textureId = -1;
        lanternMat.roughness = 0.3f;
        lanternMat.metallic  = 0.6f;
        am.addMaterial("lantern_mat", lanternMat);

        Core::Material groundMat{};
        groundMat.textureId = -1;
        groundMat.roughness = 0.9f;
        groundMat.metallic  = 0.0f;
        am.addMaterial("ground_mat", groundMat);

        Core::Material streetMat{};
        streetMat.textureId = -1;
        streetMat.roughness = 0.8f;
        streetMat.metallic  = 0.0f;
        am.addMaterial("street_mat", streetMat);

        // ── Wizard skeleton model + animations ────────────────
        const std::string gltfPath = "C:/Users/robry/Desktop/3DModels/Wizard.gltf";
        model = am.loadSkeletonModel("Wizard", gltfPath);
        animNames = am.loadAnimations("Wizard", gltfPath);
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

        // Simulate a heavy initial load
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    void setupScene(flecs::world& world) override {
        using namespace Lca;

        auto& am  = Core::GetAssetManager();
        auto& ren = Core::GetRenderer();

        uint32_t pipelineId     = ren.getSkeletonMeshPipelineId("skeletonPBR");
        uint32_t meshPipelineId = ren.getMeshPipelineId("basic");
        constexpr float playerCapsuleHeight = 1.0f;
        constexpr float playerCapsuleRadius = 0.3f;

        // ── Skybox ────────────────────────────────────────────
        {
            auto e = world.entity("Skybox");
            e.add<Component::Persistent>();
            e.set(Component::Transform{
                glm::vec3(0.0f),
                glm::radians(-90.0f), glm::vec3(1,0,0),
                glm::vec3(400.0f)});
            e.add<Component::TransformID>();
            e.add<Component::Static>();

            auto m = world.entity("SkyboxMesh");
            m.add<Component::Persistent>();
            m.add(flecs::ChildOf, e);
            m.set<Component::Mesh>({
                am.getMeshId("skyboxMesh"),
                am.getMaterialId("skyboxMat"),
                meshPipelineId
            });
        }

        // ── Static rigid-body floor (very large, covers the entire city grid) ──
        {
            glm::vec3 floorHalfExtent(500.0f, 0.5f, 500.0f);
            auto floor = world.entity("Floor");
            floor.add<Component::Persistent>();
            floor.set(Component::Transform{glm::vec3(0.0f, -0.5f, 0.0f), 0.0f, glm::vec3(0,1,0), glm::vec3(floorHalfExtent)});
            floor.add<Component::TransformID>();
            floor.set<Component::BoxCollider>({ floorHalfExtent });
            floor.set<Component::StaticRigidBody>({ .objectLayer = Layers::STATIC_ENVIRONMENT, .friction = 0.8f, .restitution = 0.0f });
        }

        // ── Wizard player entity ──────────────────────────────
        auto player = world.entity("Player");
        player.add<Component::Persistent>();
        const float floorTopY = 0.0f;
        const float playerCenterY = floorTopY + (playerCapsuleHeight * 0.5f) + playerCapsuleRadius;
        const float meshLocalYOffset = -(playerCapsuleHeight * 0.5f + playerCapsuleRadius);
        player.set(Component::Transform{glm::vec3(0.0f, playerCenterY, 0.0f), 0.0f, glm::vec3(0,1,0), glm::vec3(1.0f)});
        player.set(Component::MeshTransform{Component::Transform{glm::vec3(0.0f, meshLocalYOffset, 0.0f), 0.0f, glm::vec3(0,1,0), glm::vec3(1.0f)}});
        player.add<Component::TransformID>();
        player.add<Component::SkeletonInstanceID>();

        for (size_t k = 0; k < model.size(); ++k) {
            const auto& meshInst = model[k];
            auto child = world.entity(("PlayerMesh_" + std::to_string(k)).c_str());
            child.add<Component::Persistent>();
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
            .multi_threaded()
            .kind(flecs::PreUpdate)
            .each([](CharacterController& ctrl, Component::CharacterCapsule& capsule,
                     Component::AnimationStateMachine& asm_) {
                ctrl.update(capsule, asm_);
            });

        // ── Camera following the player ───────────────────────
        world.entity("Camera")
            .add<Component::Persistent>()
            .set<Component::CharacterCamera>({
                .target        = player,
                .yaw           = glm::pi<float>(),
                .pitch         = 0.15f,
                .distance      = 8.0f,
                .rotationSpeed = 0.005f,
                .fov           = glm::radians(60.0f),
                .nearClip      = 0.1f,
                .farClip       = 3000.0f,
                .targetOffset  = glm::vec3(0.0f, 1.5f, 0.0f)
            })
            .add<Component::Transform>();

        // ── Sun light ─────────────────────────────────────────
        world.entity("Sun")
            .add<Component::Persistent>()
            .set<Component::DirectionalLight>({
                .color     = glm::vec3(1.0f, 0.98f, 0.9f),
                .intensity = 2.0f,
            });

        // ── Spawn 500 NPC wizards that wander the city ────────
        {
            constexpr int kNpcCount = 500;
            constexpr float npcCapsuleHeight = 1.0f;
            constexpr float npcCapsuleRadius = 0.3f;
            const float npcCenterY = floorTopY + (npcCapsuleHeight * 0.5f) + npcCapsuleRadius;
            const float npcMeshYOff = -(npcCapsuleHeight * 0.5f + npcCapsuleRadius);

            // Build a template ASM identical to the player's
            // (reuses the same animation names already resolved above)
            auto buildNpcASM = [&]() -> Component::AnimationStateMachine {
                Component::AnimationStateMachine a;
                a.skeletonName = "Wizard";

                if (!idleName.empty() && !walkName.empty() && !runName.empty()) {
                    Component::BlendSpace1D bs;
                    bs.pointCount = 3;
                    bs.animationNames[0] = animKey(idleName);
                    bs.animationNames[1] = animKey(walkName);
                    bs.animationNames[2] = animKey(runName);
                    bs.animationPoints[0] = 0.0f;
                    bs.animationPoints[1] = 0.5f;
                    bs.animationPoints[2] = 1.0f;
                    bs.blendParameter     = 0.0f;
                    bs.animationSpeed[0]  = 1.0f;
                    bs.animationSpeed[1]  = 1.0f;
                    bs.animationSpeed[2]  = 1.0f;
                    a.addState("IdleWalkRun", bs);
                    a.setCurrentState("IdleWalkRun");
                } else if (!idleName.empty()) {
                    Component::SingleAnimation idle;
                    idle.animationName  = animKey(idleName);
                    idle.animationSpeed = 1.0f;
                    idle.looping        = true;
                    a.addState("IdleWalkRun", idle);
                    a.setCurrentState("IdleWalkRun");
                }

                if (!deathName.empty()) {
                    Component::SingleAnimation death;
                    death.animationName  = animKey(deathName);
                    death.animationSpeed = 1.0f;
                    death.looping        = false;
                    a.addState("Death", death);
                }
                return a;
            };

            ZoneRng npcRng(42u);
            for (int i = 0; i < kNpcCount; ++i) {
                // Spread NPCs across the whole city (roughly +-200 units)
                float nx = npcRng.nextFloat(-200.0f, 200.0f);
                float nz = npcRng.nextFloat(-200.0f, 200.0f);

                std::string npcName = "NPC_" + std::to_string(i);
                auto npc = world.entity(npcName.c_str());
                npc.set(Component::Transform{
                    glm::vec3(nx, npcCenterY, nz),
                    0.0f, glm::vec3(0,1,0), glm::vec3(1.0f)});
                npc.set(Component::MeshTransform{Component::Transform{
                    glm::vec3(0.0f, npcMeshYOff, 0.0f),
                    0.0f, glm::vec3(0,1,0), glm::vec3(1.0f)}});
                npc.add<Component::TransformID>();
                npc.add<Component::SkeletonInstanceID>();

                // Skeleton mesh children (same sub-meshes as the player)
                for (size_t k = 0; k < model.size(); ++k) {
                    const auto& mi = model[k];
                    auto child = world.entity((npcName + "_Mesh_" + std::to_string(k)).c_str());
                    child.add(flecs::ChildOf, npc);
                    child.set<Component::SkeletonMesh>({
                        .skeletonName = "Wizard",
                        .meshID       = mi.meshId,
                        .materialID   = mi.materialId,
                        .pipelineID   = pipelineId,
                        .nodeIndex    = mi.nodeIndex
                    });
                }

                // Physics capsule
                npc.set<Component::CharacterCapsule>({
                    .capsuleHeight  = npcCapsuleHeight,
                    .capsuleRadius  = npcCapsuleRadius,
                    .layer          = Layers::ENEMY_BODY,
                    .speedDecrement = 4.0f
                });

                // Animation
                npc.set(buildNpcASM());

                // NPC AI controller — each gets unique RNG seed & varied pace
                {
                    NpcController ctrl;
                    ctrl.rng = static_cast<uint32_t>(i * 7919u + 12345u);
                    ctrl.preferredPace = npcRng.nextFloat(0.25f, 0.75f);
                    // Start walking immediately toward a nearby waypoint
                    ctrl.idleTimer = npcRng.nextFloat(0.0f, 3.0f);
                    ctrl.waypoint = glm::vec3(
                        nx + npcRng.nextFloat(-30.0f, 30.0f),
                        npcCenterY,
                        nz + npcRng.nextFloat(-30.0f, 30.0f));
                    npc.set(ctrl);
                }
            }

            std::cout << "[Example6] Spawned " << kNpcCount << " NPC wizards.\n";
        }

        // ── NPC Controller system ─────────────────────────────
        world.system<NpcController, Component::CharacterCapsule,
                     Component::AnimationStateMachine, const Component::Transform>(
            "NPC Controller Update")
            .kind(flecs::PreUpdate)
            .multi_threaded()
            .each([](flecs::entity e, NpcController& ctrl,
                     Component::CharacterCapsule& capsule,
                     Component::AnimationStateMachine& asm_,
                     const Component::Transform& tf) {
                ctrl.update(capsule, asm_, tf);
            });

        // ── Register the zone registry singleton and build 10x10 zones ──
        world.component<ZoneRegistry>();
        auto& reg = world.ensure<ZoneRegistry>();

        const float zoneW  = ZoneRegistry::kZoneCols * ZoneRegistry::kSpacing;
        const float zoneH  = ZoneRegistry::kZoneRows * ZoneRegistry::kSpacing;
        const float totalW = ZoneRegistry::kGridX * zoneW + (ZoneRegistry::kGridX - 1) * ZoneRegistry::kGap;
        const float totalH = ZoneRegistry::kGridZ * zoneH + (ZoneRegistry::kGridZ - 1) * ZoneRegistry::kGap;
        const float startX = -totalW * 0.5f;
        const float startZ = -totalH * 0.5f;

        const float centreCol = (ZoneRegistry::kGridX - 1) * 0.5f;   // 4.5
        const float centreRow = (ZoneRegistry::kGridZ - 1) * 0.5f;

        for (int row = 0; row < ZoneRegistry::kGridZ; ++row) {
            for (int col = 0; col < ZoneRegistry::kGridX; ++col) {
                float ox = startX + col * (zoneW + ZoneRegistry::kGap);
                float oz = startZ + row * (zoneH + ZoneRegistry::kGap);

                glm::vec4 wallCol   = ZoneHelpers::colorFromGrid(col, row, 0.45f, 0.7f, 0.95f);
                glm::vec4 carCol    = ZoneHelpers::colorFromGrid(col, row, 0.8f,  0.4f, 0.8f);
                glm::vec3 lightCol  = glm::vec3(ZoneHelpers::colorFromGrid(col, row, 0.9f, 0.6f, 1.0f));
                glm::vec4 groundCol = ZoneHelpers::colorFromGrid(col, row, 0.3f,  0.25f, 0.45f);

                // Zone type based on distance from grid centre
                float dx = col - centreCol;
                float dz = row - centreRow;
                float dist = std::sqrt(dx * dx + dz * dz);
                int zoneType;
                if (dist < 2.0f)      zoneType = 2;  // downtown
                else if (dist < 4.0f) zoneType = 1;  // commercial
                else                  zoneType = 0;  // residential

                reg.zone(col, row) = std::make_shared<ZoneStreamingLevel>(
                    ZoneHelpers::zoneId(col, row), ox, oz,
                    ZoneRegistry::kZoneRows, ZoneRegistry::kZoneCols,
                    wallCol, carCol, lightCol, groundCol,
                    zoneType
                );
            }
        }

        // ── Proximity streaming system (runs every frame via ECS) ──
        // Tracks the Player entity's Transform position for zone streaming.
        // Filter on CharacterController to match only the player, not NPCs.
        world.system<const Component::Transform, const Component::CharacterCapsule>("ZoneProximityStreamer")
            .with<CharacterController>()
            .each([](flecs::entity e, const Component::Transform& tf, const Component::CharacterCapsule&) {
                auto w = e.world();
                if (!w.has<ZoneRegistry>()) return;
                const auto& reg = w.get<ZoneRegistry>();

                glm::vec2 camPos(tf.position.x, tf.position.z);
                float inR2  = reg.streamInRadius  * reg.streamInRadius;
                float outR2 = reg.streamOutRadius * reg.streamOutRadius;

                Lca::StreamInRequest  inReq;
                Lca::StreamOutRequest outReq;

                for (int i = 0; i < ZoneRegistry::kGridX * ZoneRegistry::kGridZ; ++i) {
                    auto& zone = reg.zones[i];
                    if (!zone) continue;

                    glm::vec2 center = zone->getCenter();
                    glm::vec2 diff   = camPos - center;
                    float dist2 = glm::dot(diff, diff);

                    // Application deduplicates streamIn/streamOut calls automatically.
                    if (dist2 <= inR2) {
                        inReq.levels.push_back(zone);
                    } else if (dist2 > outR2) {
                        outReq.levelIds.push_back(zone->getId());
                    }
                }

                if (!inReq.levels.empty()) {
                    if (w.has<Lca::StreamInRequest>()) {
                        auto& existing = w.get_mut<Lca::StreamInRequest>();
                        for (auto& l : inReq.levels) existing.levels.push_back(l);
                    } else {
                        w.set<Lca::StreamInRequest>(std::move(inReq));
                    }
                }
                if (!outReq.levelIds.empty()) {
                    if (w.has<Lca::StreamOutRequest>()) {
                        auto& existing = w.get_mut<Lca::StreamOutRequest>();
                        for (auto& id : outReq.levelIds) existing.levelIds.push_back(id);
                    } else {
                        w.set<Lca::StreamOutRequest>(std::move(outReq));
                    }
                }
            });
    }

    void cleanupScene(flecs::world& world) override {
        // Remove the ZoneRegistry singleton so zone shared_ptrs are released
        world.remove<ZoneRegistry>();

        // Delete all non-persistent entities (proximity system, etc.)
        deleteNonPersistentEntities(world);
    }

private:
    Core::Model model;
    std::vector<std::string> animNames;
};
