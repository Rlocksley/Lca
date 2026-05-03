#pragma once

#include "Global.hpp"
#include "flecs.h"
#include "Persistent.hpp"
#include "Time.hpp"
#include "Transform.hpp"
#include "CharacterCapsule.hpp"
#include "../core/Navigation.hpp"

namespace Lca {
namespace Component {

struct NavAgent {
    glm::vec3 projectionHalfExtents{2.0f, 4.0f, 2.0f};
    float repathInterval{0.25f};
    float cornerReachDistance{0.5f};
    float maxSpeed{3.0f};
    float repathTimer{0.0f};
    bool requestRepath{true};
};

struct NavTarget {
    glm::vec3 worldPos{0.0f};
    bool active{false};
};

struct NavPath {
    std::vector<glm::vec3> corners;
    uint32_t nextCorner{0};
    bool valid{false};
};

struct NavSteering {
    glm::vec3 desiredDirection{0.0f};
    glm::vec3 desiredVelocity{0.0f};
    bool reachedGoal{false};
};

} // namespace Component

namespace Module {

struct NavMesh {
    NavMesh(flecs::world& world) {
        world.component<Component::NavAgent>();
        world.component<Component::NavTarget>();
        world.component<Component::NavPath>();
        world.component<Component::NavSteering>();

        // Query a new path when needed and cache the straight-path corners.
        world.system<Component::NavAgent, const Component::NavTarget, const Component::Transform, Component::NavPath>("NavMeshPathQuery")
            .kind(flecs::PreUpdate)
            .each([](flecs::entity e,
                     Component::NavAgent& agent,
                     const Component::NavTarget& target,
                     const Component::Transform& transform,
                     Component::NavPath& path) {
                auto& nav = Core::GetNavigation();

                if (!target.active || !nav.isInitialized()) {
                    path.corners.clear();
                    path.nextCorner = 0;
                    path.valid = false;
                    return;
                }

                if (!agent.requestRepath) {
                    if (path.valid) {
                        return;
                    }

                    agent.repathTimer -= Time::deltaTime;
                    if (agent.repathTimer > 0.0f) {
                        return;
                    }
                }

                std::vector<glm::vec3> straightPath;
                const bool ok = nav.findPath(
                    transform.position,
                    target.worldPos,
                    straightPath,
                    agent.projectionHalfExtents
                );

                path.corners = std::move(straightPath);
                path.nextCorner = 0;
                path.valid = ok && !path.corners.empty();

                agent.requestRepath = false;
                agent.repathTimer = path.valid ? agent.repathInterval : 0.5f;
            })
            .add<Component::PersistentSystem>();

        // Convert path corners into steering vectors for controllers.
        world.system<Component::NavAgent, const Component::NavTarget, const Component::Transform, Component::NavPath, Component::NavSteering>("NavMeshSteering")
            .kind(flecs::PreUpdate)
            .each([](flecs::entity e,
                     Component::NavAgent& agent,
                     const Component::NavTarget& target,
                     const Component::Transform& transform,
                     Component::NavPath& path,
                     Component::NavSteering& steering) {
                steering.desiredDirection = glm::vec3(0.0f);
                steering.desiredVelocity = glm::vec3(0.0f);
                steering.reachedGoal = false;

                if (!target.active || !path.valid || path.corners.empty()) {
                    return;
                }

                while (path.nextCorner < path.corners.size()) {
                    glm::vec3 toCorner = path.corners[path.nextCorner] - transform.position;
                    toCorner.y = 0.0f;
                    if (glm::length(toCorner) > agent.cornerReachDistance) {
                        break;
                    }
                    path.nextCorner++;
                }

                if (path.nextCorner >= path.corners.size()) {
                    steering.reachedGoal = true;
                    path.valid = false;
                    agent.requestRepath = false;
                    return;
                }

                glm::vec3 toCorner = path.corners[path.nextCorner] - transform.position;
                toCorner.y = 0.0f;
                const float len = glm::length(toCorner);
                if (len <= 0.0001f) {
                    return;
                }

                steering.desiredDirection = toCorner / len;
                steering.desiredVelocity = steering.desiredDirection * agent.maxSpeed;
            })
            .add<Component::PersistentSystem>();

        // Optional bridge: write nav steering into CharacterCapsule movement if present.
        world.system<const Component::NavSteering, Component::CharacterCapsule>("NavMeshApplyToCharacterCapsule")
            .kind(flecs::PreUpdate)
            .each([](flecs::entity e,
                     const Component::NavSteering& steering,
                     Component::CharacterCapsule& capsule) {
                if (!capsule.character) {
                    return;
                }

                if (steering.reachedGoal || glm::length(steering.desiredDirection) <= 0.0001f) {
                    capsule.isRunning = false;
                    capsule.decrSpeed();
                    return;
                }

                const float desiredSpeed = glm::length(steering.desiredVelocity);
                capsule.isRunning = desiredSpeed > capsule.maxWalkSpeed;

                // Smoothly approach the desired speed without oscillation
                if (capsule.moveSpeed < desiredSpeed) {
                    capsule.moveSpeed = std::min(
                        capsule.moveSpeed + capsule.speedIncrement * Time::deltaTime,
                        desiredSpeed);
                } else if (capsule.moveSpeed > desiredSpeed + 0.01f) {
                    capsule.moveSpeed = std::max(
                        capsule.moveSpeed - capsule.speedDecrement * Time::deltaTime,
                        desiredSpeed);
                }

                // Rotate capsule to face movement direction
                float targetYaw = std::atan2(steering.desiredDirection.x,
                                             steering.desiredDirection.z);
                capsule.rotation = JPH::Quat::sRotation(
                    JPH::Vec3(0.0f, 1.0f, 0.0f), targetYaw);

                capsule.move(steering.desiredDirection);
            })
            .add<Component::PersistentSystem>();
    }
};

} // namespace Module
} // namespace Lca
