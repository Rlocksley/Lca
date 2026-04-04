#pragma once

#include "Global.hpp"
#include "flecs.h"
#include "Transform.hpp"
#include "MeshTransform.hpp"
#include "Renderer.hpp"
#include "Persistent.hpp"

namespace Lca{
    namespace Component {
        struct TransformID {
            uint32_t id;

            TransformID() : id(UINT32_MAX) {}
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
                
                // Observer: Transform + TransformID (no MeshTransform)
                world.observer<Lca::Component::Transform, Lca::Component::TransformID>()
                .without<Lca::Component::MeshTransform>()
                .event(flecs::OnAdd)
                .event(flecs::OnSet)
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
                    
                    if (transformID.id == UINT32_MAX) {
                        transformID.id = Core::GetRenderer().addModelMatrix(modelMatrix);
                    } else {
                        Core::GetRenderer().updateModelMatrix(transformID.id, modelMatrix);
                    }
                })
                .add<Lca::Component::PersistentSystem>();

                // Observer: Transform + MeshTransform + TransformID
                world.observer<Lca::Component::Transform, Lca::Component::MeshTransform, Lca::Component::TransformID>()
                .event(flecs::OnAdd)
                .event(flecs::OnSet)
                .each([](flecs::entity e, Lca::Component::Transform& transform, Lca::Component::MeshTransform& meshTransform, Lca::Component::TransformID& transformID) {
                    Lca::Component::Transform combined = transform * meshTransform.local;

                    Core::ModelMatrix modelMatrix;
                    modelMatrix.model = combined.getMatrix();
                    modelMatrix.normal = glm::transpose(glm::inverse(modelMatrix.model));
                    modelMatrix.position = glm::vec4(combined.position, 0.0f);
                    modelMatrix.scale = glm::vec4(combined.scale, 0.0f);
                    modelMatrix.rotation.x = combined.rotation.x;
                    modelMatrix.rotation.y = combined.rotation.y;
                    modelMatrix.rotation.z = combined.rotation.z;
                    modelMatrix.rotation.w = combined.rotation.w;
                    
                    if (transformID.id == UINT32_MAX) {
                        transformID.id = Core::GetRenderer().addModelMatrix(modelMatrix);
                    } else {
                        Core::GetRenderer().updateModelMatrix(transformID.id, modelMatrix);
                    }
                })
                .add<Lca::Component::PersistentSystem>();

                world.observer<Lca::Component::TransformID>()
                .event(flecs::OnRemove)
                .each([](flecs::entity e, Lca::Component::TransformID& transformID) {
                    Core::GetRenderer().removeModelMatrix(transformID.id);
                })
                .add<Lca::Component::PersistentSystem>();

                // System: Transform + TransformID (no MeshTransform)
                world.system<Lca::Component::Transform, Lca::Component::TransformID>("Non Static Transform Update")
                .without<Lca::Component::Static>()
                .without<Lca::Component::MeshTransform>()
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
                })
                .add<Lca::Component::PersistentSystem>();

                // System: Transform + MeshTransform + TransformID
                world.system<Lca::Component::Transform, Lca::Component::MeshTransform, Lca::Component::TransformID>("Non Static MeshTransform Update")
                .without<Lca::Component::Static>()
                .run([](flecs::iter& it) {
                    auto& renderer = Core::GetRenderer();

                    while(it.next()) {
                        const auto transforms = it.field<Lca::Component::Transform>(0);
                        const auto meshTransforms = it.field<Lca::Component::MeshTransform>(1);
                        const auto transformIDs = it.field<Lca::Component::TransformID>(2);

                        for (int i = 0; i < it.count(); i++) {
                            Lca::Component::Transform combined = transforms[i] * meshTransforms[i].local;

                            Core::ModelMatrix modelMatrix;
                            modelMatrix.model = combined.getMatrix();
                            modelMatrix.normal = glm::transpose(glm::inverse(modelMatrix.model));
                            modelMatrix.position = glm::vec4(combined.position, 0.0f);
                            modelMatrix.scale = glm::vec4(combined.scale, 0.0f);
                            modelMatrix.rotation.x = combined.rotation.x;
                            modelMatrix.rotation.y = combined.rotation.y;
                            modelMatrix.rotation.z = combined.rotation.z;
                            modelMatrix.rotation.w = combined.rotation.w;

                            renderer.updateModelMatrix(transformIDs[i].id, modelMatrix);
                        }
                    }
                })
                .add<Lca::Component::PersistentSystem>();

                world.system("Copy Model Matrices to GPU")
                .each([](){
                    Core::GetRenderer().copyModelMatricesToGPU(Core::currentFrameIndex);
                })
                .add<Lca::Component::PersistentSystem>();
            }

        };
    }
}