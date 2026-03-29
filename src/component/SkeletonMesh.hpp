#pragma once

#include "Global.hpp"
#include "flecs.h"
#include "TransformID.hpp"
#include "Transform.hpp"
#include "Renderer.hpp"
#include "Hidden.hpp"
#include "Persistent.hpp"

namespace Lca {
    namespace Component {

        struct SkeletonMesh {
            std::string skeletonName;
            uint32_t meshID;
            uint32_t materialID;
            uint32_t pipelineID;
            int32_t nodeIndex{-1}; // -1 for deformed, >= 0 for node-attached
        };

        struct SkeletonMeshRender {
            uint32_t skeletonMeshInstanceID{UINT32_MAX};
        };

        struct SkeletonInstanceID {
            uint32_t id{UINT32_MAX};
        };
    }

    namespace Module {
        struct SkeletonMesh {
            SkeletonMesh(flecs::world& world) {
                world.component<Lca::Component::SkeletonMesh>();
                world.component<Lca::Component::SkeletonMeshRender>();
                world.component<Lca::Component::SkeletonInstanceID>();

                // Allocate a SkeletonInstance on the parent when SkeletonInstanceID is added
                world.observer<Lca::Component::SkeletonInstanceID>()
                    .event(flecs::OnAdd)
                    .each([](flecs::entity e, Lca::Component::SkeletonInstanceID& skelInstID) {
                        if (skelInstID.id == UINT32_MAX) {
                            skelInstID.id = Core::GetRenderer().addSkeletonInstance();
                        }
                    })
                    .add<Lca::Component::PersistentSystem>();

                // Free the SkeletonInstance when SkeletonInstanceID is removed from the parent
                world.observer<Lca::Component::SkeletonInstanceID>()
                    .event(flecs::OnRemove)
                    .each([](flecs::entity e, Lca::Component::SkeletonInstanceID& skelInstID) {
                        if (skelInstID.id != UINT32_MAX) {
                            Core::GetRenderer().removeSkeletonInstance(skelInstID.id);
                        }
                    })
                    .add<Lca::Component::PersistentSystem>();

                // Observer: when SkeletonMesh is set (child entity), create/update SkeletonMeshInstance
                // Parent must have Transform, TransformID, and SkeletonInstanceID
                world.observer<const Lca::Component::SkeletonMesh, const Lca::Component::Transform, const Lca::Component::TransformID, const Lca::Component::SkeletonInstanceID>()
                    .term_at(1).parent()
                    .term_at(2).parent()
                    .term_at(3).parent()
                    .event(flecs::OnSet)
                    .each([](flecs::entity e, const Lca::Component::SkeletonMesh& skelMesh, const Lca::Component::Transform& transform, const Lca::Component::TransformID& transformID, const Lca::Component::SkeletonInstanceID& skelInstID) {
                        if (e.has<Lca::Component::Hidden>()) return;

                        Core::SkeletonMeshInstance instance;
                        instance.transformID = transformID.id;
                        instance.shaderID = skelMesh.pipelineID;
                        instance.meshID = skelMesh.meshID;
                        instance.materialID = skelMesh.materialID;
                        instance.skeletonInstanceID = skelInstID.id;
                        instance.nodeIndex = skelMesh.nodeIndex;

                        if (e.has<Lca::Component::SkeletonMeshRender>()) {
                            const auto& render = e.get<Lca::Component::SkeletonMeshRender>();
                            Core::GetRenderer().updateSkeletonMeshInstance(render.skeletonMeshInstanceID, instance);
                        } else {
                            uint32_t id = Core::GetRenderer().addSkeletonMeshInstance(instance);
                            e.set<Lca::Component::SkeletonMeshRender>({ id });
                        }
                    })
                    .add<Lca::Component::PersistentSystem>();

                // Observer: cleanup on SkeletonMesh removal
                world.observer<const Lca::Component::SkeletonMesh>()
                    .event(flecs::OnRemove)
                    .each([](flecs::entity e, const Lca::Component::SkeletonMesh& skelMesh) {
                        if (e.has<Lca::Component::SkeletonMeshRender>()) {
                            const auto& render = e.get<Lca::Component::SkeletonMeshRender>();
                            Core::GetRenderer().removeSkeletonMeshInstance(render.skeletonMeshInstanceID);
                            e.remove<Lca::Component::SkeletonMeshRender>();
                        }
                    })
                    .add<Lca::Component::PersistentSystem>();

                // Hidden handling for skeleton meshes
                world.observer<const Lca::Component::Hidden>()
                    .event(flecs::OnAdd)
                    .each([](flecs::entity e, const Lca::Component::Hidden&) {
                        if (e.has<Lca::Component::SkeletonMeshRender>()) {
                            const auto& render = e.get<Lca::Component::SkeletonMeshRender>();
                            Core::GetRenderer().removeSkeletonMeshInstance(render.skeletonMeshInstanceID);
                            e.remove<Lca::Component::SkeletonMeshRender>();
                        }
                    })
                    .add<Lca::Component::PersistentSystem>();

                world.observer<const Lca::Component::Hidden>()
                    .event(flecs::OnRemove)
                    .each([](flecs::entity e, const Lca::Component::Hidden&) {
                        if (e.has<Lca::Component::SkeletonMesh>()) {
                            e.modified<Lca::Component::SkeletonMesh>();
                        }
                    })
                    .add<Lca::Component::PersistentSystem>();

                // System: copy skeleton mesh instances to GPU
                world.system("Copy Skeleton Mesh Instances to GPU")
                    .each([]() {
                        Core::GetRenderer().copySkeletonMeshInstancesToGPU(Core::currentFrameIndex);
                    })
                    .add<Lca::Component::PersistentSystem>();
            }
        };
    }
}
