#pragma once

#include "Global.hpp"
#include "Time.hpp"
#include "flecs.h"
#include "TransformID.hpp"
#include "Transform.hpp"
#include "Renderer.hpp"
#include "Hidden.hpp"
#include "Persistent.hpp"

namespace Lca {
    namespace Component {

        // ── User-facing component ────────────────────────────────────────────
        // Place this on a child entity whose parent owns a TransformID.
        // Mirrors the role that Mesh plays for static geometry.
        struct ParticleSystem {
            uint32_t  meshID;              // billboard / shape mesh
            uint32_t  materialID;
            uint32_t  computePipelineID;   // ParticleSystemCompPipeline variant index
            uint32_t  graphicsPipelineID;  // ParticleSystemPipeline variant index
            uint32_t  particleCount{256};
            float     boundingSphereRadius{5.0f};  // world-space, used by cull shader
            glm::mat4 localOffset{1.0f};            // offset from parent transform
        };

        // ── Internal bookkeeping ─────────────────────────────────────────────
        // Added by the module when the Renderer slot is allocated.
        struct ParticleSystemRender {
            uint32_t systemSlotID{UINT32_MAX};
        };
    }

    namespace Module {
        struct ParticleSystem {
            ParticleSystem(flecs::world& world) {
                world.component<Lca::Component::ParticleSystem>();
                world.component<Lca::Component::ParticleSystemRender>();

                // ── OnSet: allocate or update the GPU-side particle system ───
                // Triggered when ParticleSystem is first set or mutated on a
                // child entity.  Transform and TransformID are read from the parent.
                world.observer<const Lca::Component::ParticleSystem,
                               const Lca::Component::Transform,
                               const Lca::Component::TransformID>()
                    .term_at(1).parent()
                    .term_at(2).parent()
                    .event(flecs::OnSet)
                    .each([](flecs::entity e,
                             const Lca::Component::ParticleSystem&   ps,
                             const Lca::Component::Transform&         /*transform*/,
                             const Lca::Component::TransformID&       transformID)
                    {
                        if (e.has<Lca::Component::Hidden>()) return;

                        Core::ParticleSystemInstance instance;
                        instance.transformID         = transformID.id;
                        instance.computeID           = ps.computePipelineID;
                        instance.graphicsID          = ps.graphicsPipelineID;
                        instance.meshID              = ps.meshID;
                        instance.materialID          = ps.materialID;
                        instance.particleCount       = ps.particleCount;
                        instance.boundingSphereRadius= ps.boundingSphereRadius;
                        instance.localOffset         = ps.localOffset;
                        // particleOffset, indexCount, firstIndex, vertexOffset are
                        // filled by the Renderer inside addParticleSystem.
                        instance.active              = 1u;

                        if (e.has<Lca::Component::ParticleSystemRender>()) {
                            const auto& render = e.get<Lca::Component::ParticleSystemRender>();
                            Core::GetRenderer().updateParticleSystem(render.systemSlotID, instance);
                        } else {
                            uint32_t id = Core::GetRenderer().addParticleSystem(instance);
                            e.set<Lca::Component::ParticleSystemRender>({ id });
                        }
                    })
                    .add<Lca::Component::PersistentSystem>();

                // ── OnRemove: release the GPU slot ───────────────────────────
                world.observer<const Lca::Component::ParticleSystem>()
                    .event(flecs::OnRemove)
                    .each([](flecs::entity e, const Lca::Component::ParticleSystem&) {
                        if (e.has<Lca::Component::ParticleSystemRender>()) {
                            const auto& render = e.get<Lca::Component::ParticleSystemRender>();
                            Core::GetRenderer().removeParticleSystem(render.systemSlotID);
                            e.remove<Lca::Component::ParticleSystemRender>();
                        }
                    })
                    .add<Lca::Component::PersistentSystem>();

                // ── Hidden added: remove from renderer (stop rendering) ──────
                world.observer<const Lca::Component::Hidden>()
                    .event(flecs::OnAdd)
                    .each([](flecs::entity e, const Lca::Component::Hidden&) {
                        if (e.has<Lca::Component::ParticleSystemRender>()) {
                            const auto& render = e.get<Lca::Component::ParticleSystemRender>();
                            Core::GetRenderer().removeParticleSystem(render.systemSlotID);
                            e.remove<Lca::Component::ParticleSystemRender>();
                        }
                    })
                    .add<Lca::Component::PersistentSystem>();

                // ── Hidden removed: re-register with renderer ────────────────
                world.observer<const Lca::Component::Hidden>()
                    .event(flecs::OnRemove)
                    .each([](flecs::entity e, const Lca::Component::Hidden&) {
                        if (e.has<Lca::Component::ParticleSystem>()
                         && e.parent().has<Lca::Component::Transform>()
                         && e.parent().has<Lca::Component::TransformID>())
                        {
                            e.modified<Lca::Component::ParticleSystem>();
                        }
                    })
                    .add<Lca::Component::PersistentSystem>();

                // ── Per-frame: upload delta-time + sync particles to GPU ─────
                world.system("Upload Particle System Data")
                    .each([]() {
                        Core::GetRenderer().updateParticleDeltaTime(
                            Core::currentFrameIndex, Time::deltaTime);
                        Core::GetRenderer().copyParticleSystemInstancesToGPU(
                            Core::currentFrameIndex);
                    })
                    .add<Lca::Component::PersistentSystem>();
            }
        };
    }
}
