#pragma once

#include "flecs.h"
#include "AssetManager.hpp"
#include "Transform.hpp"
#include "TransformID.hpp"
#include "Mesh.hpp"

namespace Lca {
    namespace Helpers {

        /// Creates a model entity with child mesh entities for each MeshInstance.
        /// The returned entity has Transform + TransformID. Each sub-mesh is a child
        /// entity with Component::Mesh { meshId, materialId, pipelineId }.
        /// Uses the default pipelineId for all meshes.
        inline flecs::entity createModel(flecs::world& world, const std::vector<Core::MeshInstance>& model, const uint32_t pipelineId) {
            auto parent = world.entity();
            parent.set(Component::Transform{glm::vec3(0.0f), 0.0f, glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(1.0f)});
            parent.add<Component::TransformID>();

            for (size_t i = 0; i < model.size(); ++i) {
                auto child = world.entity();
                child.add(flecs::ChildOf, parent);
                child.set<Component::Mesh>({
                    model[i].meshId,
                    model[i].materialId,
                    pipelineId
                });
            }

            return parent;
        }

        /// Creates a model entity with per-mesh pipeline overrides.
        /// pipelineOverrides maps a mesh name to a pipeline ID. If a mesh name
        /// is not found in the map, defaultPipelineId is used.
        inline flecs::entity createModel(flecs::world& world, const std::vector<Core::MeshInstance>& model, const uint32_t defaultPipelineId, const std::unordered_map<std::string, uint32_t>& pipelineOverrides) {
            auto parent = world.entity();
            parent.set(Component::Transform{glm::vec3(0.0f), 0.0f, glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(1.0f)});
            parent.add<Component::TransformID>();

            for (size_t i = 0; i < model.size(); ++i) {
                uint32_t pid = defaultPipelineId;
                auto it = pipelineOverrides.find(model[i].meshName);
                if (it != pipelineOverrides.end()) {
                    pid = it->second;
                }

                auto child = world.entity();
                child.add(flecs::ChildOf, parent);
                child.set<Component::Mesh>({
                    model[i].meshId,
                    model[i].materialId,
                    pid
                });
            }

            return parent;
        }

    } // namespace Helpers
} // namespace Lca
