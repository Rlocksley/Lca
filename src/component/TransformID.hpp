#pragma once

#include "Global.hpp"
#include "flecs.h"
#include "Transform.hpp"
#include "Renderer.hpp"

namespace Lca{
    namespace Component {
        struct TransformID {
            uint32_t id;

            TransformID() : id(0) {}
            explicit TransformID(uint32_t inID) : id(inID) {}

            bool operator==(const TransformID& other) const {
                return id == other.id;
            }

            bool operator!=(const TransformID& other) const {
                return id != other.id;
            }
        };

        struct Static{};
    }

    namespace Module{
        struct TransformID{
            TransformID(flecs::world& world){
                world.component<Lca::Component::TransformID>();
                world.component<Lca::Component::Static>();
                
                world.observer<Lca::Component::Transform, Lca::Component::TransformID>()
                .event(flecs::OnAdd)
                .each([](flecs::entity e, Lca::Component::Transform& transform, Lca::Component::TransformID& transformID) {
                    Core::ModelMatrix modelMatrix;
                    modelMatrix.model = transform.getMatrix();
                    modelMatrix.normal = glm::transpose(glm::inverse(modelMatrix.model));
                    modelMatrix.position = glm::vec4(transform.position, 0.0f);
                    modelMatrix.scale = glm::vec4(transform.scale, 0.0f);
                    modelMatrix.rotation.x = transform.rotation.x;
                    modelMatrix.rotation.y = transform.rotation.y;
                    modelMatrix.rotation.z = transform.rotation.z;
                    modelMatrix.rotation.w = transform.rotation.w;
                    
                    transformID.id = Core::GetRenderer().addModelMatrix(modelMatrix);
                });

                world.observer<Lca::Component::TransformID>()
                .event(flecs::OnRemove)
                .each([](flecs::entity e, Lca::Component::TransformID& transformID) {
                    Core::GetRenderer().removeModelMatrix(transformID.id);
                });

                world.system<Lca::Component::Transform, Lca::Component::TransformID>()
                .without<Lca::Component::Static>()
                .run([](flecs::iter& it) {
                    auto& renderer = Core::GetRenderer();

                    while(it.next()) {
                        const auto transforms = it.field<Lca::Component::Transform>(0);
                        const auto transformIDs = it.field<Lca::Component::TransformID>(1);

                        for (int i = 0; i < it.count(); i++) {
                            Core::ModelMatrix modelMatrix;
                            modelMatrix.model = transforms[i].getMatrix();
                            modelMatrix.normal = glm::transpose(glm::inverse(modelMatrix.model));
                            modelMatrix.position = glm::vec4(transforms[i].position, 0.0f);
                            modelMatrix.scale = glm::vec4(transforms[i].scale, 0.0f);
                            modelMatrix.rotation.x = transforms[i].rotation.x;
                            modelMatrix.rotation.y = transforms[i].rotation.y;
                            modelMatrix.rotation.z = transforms[i].rotation.z;
                            modelMatrix.rotation.w = transforms[i].rotation.w;

                            renderer.updateModelMatrix(transformIDs[i].id, modelMatrix);
                        }
                    }
                });
            }

        };
    }
}