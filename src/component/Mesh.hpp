#pragma once

#include "Global.hpp"
#include "flecs.h"
#include "TransformID.hpp"
#include "Transform.hpp"
#include "Renderer.hpp"

namespace Lca{
    namespace Component{
        struct Hidden {};

        struct Mesh{
            uint32_t meshID;
            uint32_t materialID;
            uint32_t pipelineID;
        };

        struct MeshRender{
            uint32_t objectInstanceID;
        };
    }

    namespace Module{
        struct Mesh{
            Mesh(flecs::world& world){
                world.component<Lca::Component::Mesh>();

                world.observer<const Lca::Component::Mesh, const Lca::Component::Transform, const Lca::Component::TransformID>()
                    .term_at(1).parent()
                    .term_at(2).parent()
                    .event(flecs::OnSet)
                    .each([](flecs::entity e, const Lca::Component::Mesh& mesh, const Lca::Component::Transform& transform, const Lca::Component::TransformID& transformID) {
                        if(e.has<Lca::Component::Hidden>()) return;

                        Core::ObjectInstance instance;        
                        instance.transformID = transformID.id;
                        instance.shaderID = mesh.pipelineID;
                        instance.meshID = mesh.meshID;
                        instance.materialID = mesh.materialID;

                        if(e.has<Lca::Component::MeshRender>()){
                            const auto& meshRender = e.get<Lca::Component::MeshRender>();
                            Core::GetRenderer().updateObjectInstance(meshRender.objectInstanceID, instance);
                        }
                        else{
                            uint32_t id = Core::GetRenderer().addObjectInstance(instance);
                            e.set<Lca::Component::MeshRender>({ id });
                        }
                    });

                world.observer<const Lca::Component::Mesh>()
                    .event(flecs::OnRemove)
                    .each([](flecs::entity e, const Lca::Component::Mesh& mesh) {
                        if(e.has<Lca::Component::MeshRender>()){
                            const auto& meshRender = e.get<Lca::Component::MeshRender>();
                            Core::GetRenderer().removeObjectInstance(meshRender.objectInstanceID);
                            e.remove<Lca::Component::MeshRender>();
                        }
                    });

                world.observer<const Lca::Component::Hidden>()
                    .event(flecs::OnAdd)
                    .each([](flecs::entity e, const Lca::Component::Hidden&) {
                        if(e.has<Lca::Component::MeshRender>()){
                            const auto& meshRender = e.get<Lca::Component::MeshRender>();
                            Core::GetRenderer().removeObjectInstance(meshRender.objectInstanceID);
                            e.remove<Lca::Component::MeshRender>();
                        }
                    });

                world.observer<const Lca::Component::Hidden>()
                    .event(flecs::OnRemove)
                    .each([](flecs::entity e, const Lca::Component::Hidden&) {
                        if(e.has<Lca::Component::Mesh>() && e.has<Lca::Component::Transform>() && e.has<Lca::Component::TransformID>()){
                            e.modified<Lca::Component::Mesh>();
                        }
                    });

                world.system("Copy Object Instances to GPU  ")
                .each([](){
                    Core::GetRenderer().copyObjectInstancesToGPU(Core::currentFrameIndex);
                });
            }
        };
    }
}
