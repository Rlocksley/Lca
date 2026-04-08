#pragma once

#include "Level.hpp"
#include "AssetManager.hpp"
#include "Renderer.hpp"
#include "FlyingCamera.hpp"
#include "Light.hpp"
#include "Mesh.hpp"
#include "MeshTransform.hpp"
#include "ParticleSystem.hpp"
#include "Shape.hpp"
#include "Time.hpp"

using namespace Lca;

class ParticleLevel : public Lca::Level {
public:
    void setupScene(flecs::world& world) override {
        // ── Pipelines are registered in App::onInit, just need IDs here ──────
        const uint32_t meshPipeID = Core::GetRenderer().getMeshPipelineId("basic");
        const uint32_t particleCompID  = Core::GetRenderer().getParticleSystemCompId("particleSim");
        const uint32_t particleGfxID   = Core::GetRenderer().getParticleSystemPipelineId("particlePBR");
        const uint32_t fireCompID      = Core::GetRenderer().getParticleSystemCompId("particleFireSim");
        const uint32_t fireGfxID       = Core::GetRenderer().getParticleSystemPipelineId("particleFireGfx");

        const uint32_t cubeMeshID    = Core::GetAssetManager().getMeshId("cube");
        const uint32_t squareMeshID  = Core::GetAssetManager().getMeshId("square");
        const uint32_t floorMatID    = Core::GetAssetManager().getMaterialId("floorMat");
        const uint32_t particleMatID = Core::GetAssetManager().getMaterialId("particleMat");
        const uint32_t fireMatID     = Core::GetAssetManager().getMaterialId("fireMat");

        // ── Flying camera ────────────────────────────────────────────────────
        world.entity("FlyCamera")
            .set<Component::FlyingCamera>({
                .position      = glm::vec3(0.0f, 3.0f, 8.0f),
                .angle         = Component::FlyingCamera::lookAt(
                                     glm::vec3(0.0f, 3.0f, 8.0f),
                                     glm::vec3(0.0f, 1.0f, 0.0f)),
                .speed         = 8.0f,
                .rotationSpeed = 0.003f,
                .fov           = glm::radians(60.0f),
                .nearClip      = 0.05f,
                .farClip       = 500.0f
            })
            .add<Component::Transform>();

        // ── Sun ─────────────────────────────────────────────────────────────
        world.entity("Sun")
            .set<Component::DirectionalLight>({
                .color     = glm::vec3(1.0f, 0.95f, 0.85f),
                .intensity = 2.5f,
            });

        // ── Floor ────────────────────────────────────────────────────────────
        {
            auto floor = world.entity("Floor");
            floor.set<Component::Transform>({
                glm::vec3(0.0f, -0.5f, 0.0f),
                0.0f,
                glm::vec3(0, 1, 0),
                glm::vec3(12.0f, 0.2f, 12.0f)
            });
            floor.add<Component::TransformID>();

            auto floorMesh = world.entity("FloorMesh").add(flecs::ChildOf, floor);
            floorMesh.set<Component::Mesh>({
                cubeMeshID,
                floorMatID,
                meshPipeID
            });
        }

        // ── Emitter parent entity (has Transform + TransformID) ───────────────
        // The ParticleSystem component must be on a *child* of this entity.
        {
            auto emitter = world.entity("ParticleEmitter");
            emitter.set<Component::Transform>({
                glm::vec3(0.0f, 0.5f, 0.0f),   // position above the floor
                0.0f,
                glm::vec3(0, 1, 0),
                glm::vec3(1.0f)
            });
            emitter.add<Component::TransformID>();

            // Optional: show a small reference cube at the emitter origin
            auto emitterVis = world.entity("EmitterVis").add(flecs::ChildOf, emitter);
            emitterVis.set<Component::Mesh>({
                cubeMeshID,
                floorMatID,
                meshPipeID
            });

            // ── Particle system child ─────────────────────────────────────────
            // particleOffset, indexCount etc. are filled by addParticleSystem();
            // we only need to supply the IDs and tuning parameters here.
            auto particles = world.entity("Sparks").add(flecs::ChildOf, emitter);
            particles.set<Component::ParticleSystem>({
                .meshID              = cubeMeshID,    // each particle is a tiny cube
                .materialID          = particleMatID,
                .computePipelineID   = particleCompID,
                .graphicsPipelineID  = particleGfxID,
                .particleCount       = 1024,
                .boundingSphereRadius= 8.0f,
                .localOffset         = glm::mat4(1.0f)  // centred on the parent
            });
        }

        // ── Fire emitter (billboard fire particle system) ─────────────────────
        {
            auto fireEmitter = world.entity("FireEmitter");
            fireEmitter.set<Component::Transform>({
                glm::vec3(3.0f, 0.0f, 0.0f),   // offset from the spark emitter
                0.0f,
                glm::vec3(0, 1, 0),
                glm::vec3(1.0f)
            });
            fireEmitter.add<Component::TransformID>();

            auto fireParticles = world.entity("FireParticles").add(flecs::ChildOf, fireEmitter);
            fireParticles.set<Component::ParticleSystem>({
                .meshID              = squareMeshID,  // billboard quad
                .materialID          = fireMatID,     // fire sprite texture
                .computePipelineID   = fireCompID,
                .graphicsPipelineID  = fireGfxID,
                .particleCount       = 512,
                .boundingSphereRadius= 6.0f,
                .localOffset         = glm::mat4(1.0f)
            });
        }
    }
};
