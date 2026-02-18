#pragma once

#include "Global.hpp"
#include "flecs.h"
#include "TransformID.hpp"
#include "Transform.hpp"
#include "Renderer.hpp"

namespace Lca{
    namespace Component{
        struct Mesh{
            uint32_t meshID;
            uint32_t materialID;
            uint32_t pipelineID;
        };
    }

    namespace Module{
        struct Mesh{
            Mesh(flecs::world& world){
                world.component<Lca::Component::Mesh>();

                world.system<const Lca::Component::Mesh, const Lca::Component::Transform, const Lca::Component::TransformID>()
                    .term_at(1).parent()
                    .term_at(2).parent()
                    .run([](flecs::iter& it) {
                        auto& renderer = Lca::Core::GetRenderer();

                        while(it.next()) {
                            const auto meshes = it.field<const Lca::Component::Mesh>(0);
                            const auto transformIDs = it.field<const Lca::Component::TransformID>(2);

                            Lca::Core::ObjectInstance* instances = renderer.reserveObjectInstances(it.count());

                            for (int i = 0; i < it.count(); i++) {
                                instances[i].transformID = transformIDs[i].id;
                                instances[i].shaderID = meshes[i].pipelineID;
                                instances[i].meshID = meshes[i].meshID;
                                instances[i].materialID = meshes[i].materialID;
                            }
                        }
                    });
            }
        };
    }
}
