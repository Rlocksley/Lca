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
#include "ParticleSystem.hpp"
#include "Hidden.hpp"

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
// Procedural fire sprite generator (from example9)
// ──────────────────────────────────────────────────────────────
namespace ProceduralFireTex {

    static const float kPi = 3.14159265359f;

    inline float hash(float x, float y) {
        float v = std::sin(x * 127.1f + y * 311.7f) * 43758.5453f;
        return v - std::floor(v);
    }
    inline float noise(float x, float y) {
        float ix = std::floor(x), iy = std::floor(y);
        float fx = x - ix,        fy = y - iy;
        float a = hash(ix,       iy);
        float b = hash(ix + 1.f, iy);
        float c = hash(ix,       iy + 1.f);
        float d = hash(ix + 1.f, iy + 1.f);
        float ux = fx * fx * (3.f - 2.f * fx);
        float uy = fy * fy * (3.f - 2.f * fy);
        return a + (b - a) * ux + (c - a) * uy + (a - b - c + d) * ux * uy;
    }
    inline float fbm(float x, float y, int octaves = 5) {
        float val = 0.f, amp = 0.5f;
        for (int i = 0; i < octaves; ++i) {
            val += amp * noise(x, y);
            x *= 2.f; y *= 2.f;
            amp *= 0.5f;
        }
        return val;
    }
    inline uint8_t clamp8(float v) {
        int i = static_cast<int>(v * 255.f);
        return static_cast<uint8_t>(i < 0 ? 0 : (i > 255 ? 255 : i));
    }

    inline std::vector<uint32_t> generateFireSprite(int w, int h, float seed) {
        std::vector<uint32_t> pixels(w * h);
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                float u = (static_cast<float>(x) + 0.5f) / static_cast<float>(w) * 2.f - 1.f;
                float v = (static_cast<float>(y) + 0.5f) / static_cast<float>(h) * 2.f - 1.f;
                float y01 = (v + 1.0f) * 0.5f;

                float flow = fbm((u + seed * 0.7f) * 2.6f, (y01 + seed * 0.3f) * 3.4f, 4);
                float detail = fbm((u + seed * 1.9f) * 7.8f, (y01 + seed * 0.9f) * 11.2f, 3);

                float baseWidth = 0.70f;
                float tipWidth  = 0.12f;
                float width = baseWidth + (tipWidth - baseWidth) * y01;

                float swayAmp = 0.06f + y01 * 0.18f;
                float displacedU = u + (flow - 0.5f) * swayAmp;

                float coreDist = std::fabs(displacedU) / (width + 1e-4f);

                float ang = std::atan2(v, displacedU + 1e-5f);
                float tongues = std::sin(ang * 6.0f + detail * kPi * 2.0f) * 0.08f;
                coreDist += tongues * (0.2f + y01 * 0.8f);

                float verticalShape = 1.0f - y01 * y01 * 0.85f;
                verticalShape = verticalShape < 0.0f ? 0.0f : verticalShape;

                float body = 1.0f - coreDist;
                body = body < 0.0f ? 0.0f : (body > 1.0f ? 1.0f : body);
                body = body * body * (3.0f - 2.0f * body);
                float breakup = (detail - 0.5f) * (0.16f + y01 * 0.14f);
                float alpha = body * verticalShape + breakup;

                float rr = std::sqrt(u * u + v * v);
                float cornerFade = 1.0f - (rr - 0.65f) / 0.55f;
                cornerFade = cornerFade < 0.0f ? 0.0f : (cornerFade > 1.0f ? 1.0f : cornerFade);
                alpha *= cornerFade;
                alpha = alpha < 0.0f ? 0.0f : (alpha > 1.0f ? 1.0f : alpha);

                float hotCore = 1.0f - coreDist * 1.65f;
                hotCore = hotCore < 0.0f ? 0.0f : (hotCore > 1.0f ? 1.0f : hotCore);
                float baseBoost = 1.0f - y01;
                float heat = hotCore * (0.55f + baseBoost * 0.65f) + (flow - 0.5f) * 0.12f;
                heat = heat < 0.0f ? 0.0f : (heat > 1.0f ? 1.0f : heat);
                heat *= alpha;

                uint32_t px =
                    (static_cast<uint32_t>(clamp8(alpha)) << 24) |
                    (static_cast<uint32_t>(clamp8(heat))  << 16) |
                    (static_cast<uint32_t>(clamp8(heat))  <<  8) |
                     static_cast<uint32_t>(clamp8(heat));
                pixels[y * w + x] = px;
            }
        }
        return pixels;
    }

} // namespace ProceduralFireTex

// ──────────────────────────────────────────────────────────────
// Fireball — a sphere with a particle system that flies forward
// and despawns after a set lifetime.
// ──────────────────────────────────────────────────────────────

struct Fireball {
    glm::vec3 direction{0.0f, 0.0f, 1.0f};
    float     speed{15.0f};
    float     lifetime{4.0f};
    float     sphereScale{0.3f};
    float     delayTime{0.5f};   // seconds after Attack starts before spawning
};

// ──────────────────────────────────────────────────────────────
// CharacterController — reads input, drives the Jolt
// CharacterCapsule and the AnimationStateMachine blend param.
// ──────────────────────────────────────────────────────────────

struct CharacterController {
    bool  jumpHeld     = false;
    bool  wantsFireball = false;

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
            wantsFireball = true;
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
                "Spine1", "Spine2",
                "Neck", "Head",
                "LeftShoulder",  "LeftArm",  "LeftForeArm",  "LeftHand",
                "RightShoulder", "RightArm", "RightForeArm", "RightHand"
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

        // ── Fireball pipeline IDs (registered in App::onInit) ──
        const uint32_t fbCompID = ren.getParticleSystemCompId("fireballSim");
        const uint32_t fbGfxID  = ren.getParticleSystemPipelineId("fireballGfx");
        const uint32_t fbSquareMeshID = am.getMeshId("fireballSquare");
        const uint32_t fbSphereMeshID = am.getMeshId("fireballSphere");
        const uint32_t fbMatID        = am.getMaterialId("fireballMat");
        const uint32_t fbSphereMatID  = am.getMaterialId("fireballSphereMat");

        // ── Ember pipeline IDs ─────────────────────────────────
        const uint32_t emberCompID = ren.getParticleSystemCompId("fireballEmberSim");
        const uint32_t emberGfxID  = ren.getParticleSystemPipelineId("fireballEmberGfx");
        const uint32_t emberMatID  = am.getMaterialId("fireballEmberMat");

        // ── Character control system ──────────────────────────
        world.system<CharacterController, Component::CharacterCapsule, Component::AnimationStateMachine>(
            "Character Controller Update")
            .kind(flecs::PreUpdate)
            .each([](CharacterController& ctrl, Component::CharacterCapsule& capsule,
                     Component::AnimationStateMachine& asm_) {
                ctrl.update(capsule, asm_);
            });

        // ── Fireball spawning system ──────────────────────────
        // When the character controller sets wantsFireball, spawn a fireball
        // entity in front of the player and launch it forward.
        static int fireballCounter = 0;

        world.system<CharacterController, const Component::CharacterCapsule, const Component::Transform>(
            "Fireball Spawn System")
            .kind(flecs::PreUpdate)
            .each([&world, meshPipelineId, fbCompID, fbGfxID, fbSquareMeshID,
                   fbSphereMeshID, fbMatID, fbSphereMatID,
                   emberCompID, emberGfxID, emberMatID](
                      flecs::entity playerEnt,
                      CharacterController& ctrl,
                      const Component::CharacterCapsule& capsule,
                      const Component::Transform& transform)
            {
                if (!ctrl.wantsFireball) return;
                ctrl.wantsFireball = false;

                // Compute forward direction from capsule rotation
                JPH::Vec3 fwdJolt = capsule.rotation * JPH::Vec3(0.0f, 0.0f, 1.0f);
                glm::vec3 forward(fwdJolt.GetX(), 0.0f, fwdJolt.GetZ());
                if (glm::length(forward) > 0.001f) forward = glm::normalize(forward);
                else forward = glm::vec3(0.0f, 0.0f, 1.0f);

                // Spawn position: in front of the player at chest height
                glm::vec3 spawnPos = transform.position + forward * 1.5f + glm::vec3(0.0f, 1.0f, 0.0f);

                int id = fireballCounter++;
                std::string fbName = "Fireball_" + std::to_string(id);

                // Parent entity with Transform + Fireball (visuals spawn after delayTime)
                auto fbEntity = world.entity(fbName.c_str());
                fbEntity.set(Component::Transform{spawnPos, 0.0f, glm::vec3(0,1,0), glm::vec3(1.0f)});
                fbEntity.add<Component::TransformID>();
                fbEntity.set<Fireball>({ .direction = forward, .speed = 15.0f, .lifetime = 4.0f, .sphereScale = 0.7f, .delayTime = 1.0f });
                fbEntity.add<Component::Hidden>();
            });

        // ── Fireball activate system (spawns visuals after delayTime) ──
        world.system<Fireball, Component::Transform>(
            "Fireball Activate System")
            .kind(flecs::OnUpdate)
            .each([&world, meshPipelineId, fbCompID, fbGfxID, fbSquareMeshID,
                   fbSphereMeshID, fbMatID, fbSphereMatID,
                   emberCompID, emberGfxID, emberMatID](
                      flecs::entity e, Fireball& fb, Component::Transform& transform)
            {
                if (fb.delayTime <= 0.0f) return;

                fb.delayTime -= Time::deltaTime;
                if (fb.delayTime > 0.0f) return;

                // Delay expired — spawn visual children and unhide
                e.remove<Component::Hidden>();

                const std::string fbName(e.name().c_str());

                // Visible sphere mesh (the fireball core)
                auto sphereChild = world.entity((fbName + "_Sphere").c_str());
                sphereChild.add(flecs::ChildOf, e);
                sphereChild.set<Component::Mesh>({
                    fbSphereMeshID,
                    fbSphereMatID,
                    meshPipelineId
                });

                // Main fire particle system (core + flame + wisp tiers)
                auto fireChild = world.entity((fbName + "_Fire").c_str());
                fireChild.add(flecs::ChildOf, e);
                fireChild.set<Component::ParticleSystem>({
                    .meshID              = fbSquareMeshID,
                    .materialID          = fbMatID,
                    .computePipelineID   = fbCompID,
                    .graphicsPipelineID  = fbGfxID,
                    .particleCount       = 2048,
                    .boundingSphereRadius= 4.0f,
                    .localOffset         = glm::mat4(1.0f)
                });

                // Ember / spark particle system
                auto emberChild = world.entity((fbName + "_Embers").c_str());
                emberChild.add(flecs::ChildOf, e);
                emberChild.set<Component::ParticleSystem>({
                    .meshID              = fbSquareMeshID,
                    .materialID          = emberMatID,
                    .computePipelineID   = emberCompID,
                    .graphicsPipelineID  = emberGfxID,
                    .particleCount       = 512,
                    .boundingSphereRadius= 5.0f,
                    .localOffset         = glm::mat4(1.0f)
                });
            });

        // ── Fireball movement + lifetime system ───────────────
        world.system<Fireball, Component::Transform>(
            "Fireball Movement System")
            .kind(flecs::OnUpdate)
            .each([](flecs::entity e, Fireball& fb, Component::Transform& transform) {
                if (fb.delayTime > 0.0f) return; // still waiting
                transform.position += fb.direction * fb.speed * Time::deltaTime;
                fb.lifetime -= Time::deltaTime;
                if (fb.lifetime <= 0.0f) {
                    e.destruct();
                }
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
