#pragma once

#include "AnimationStateMachine.hpp"
#include "CharacterCapsule.hpp"
#include "Input.hpp"
#include "Physics.hpp"
#include "Time.hpp"
#include "Transform.hpp"

#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/RayCast.h>

#include <algorithm>

using namespace Lca;

struct CharacterController {
    bool  jumpHeld     = false;

    void update(Component::CharacterCapsule& capsule,
                Component::AnimationStateMachine& stateMachine)
    {
        if (!capsule.character) return;

        if (Input::keyA.down) capsule.turn(Time::deltaTime);
        if (Input::keyD.down) capsule.turn(-Time::deltaTime);

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

        JPH::Vec3 fwdJolt = capsule.rotation * JPH::Vec3(0.0f, 0.0f, 1.0f);
        glm::vec3 moveDir(fwdJolt.GetX(), 0.0f, fwdJolt.GetZ());
        if (glm::length(moveDir) > 0.0f) {
            moveDir = glm::normalize(moveDir);
        }
        capsule.move(moveDir);

        const bool jumpDown = Input::keySpace.down;
        if (jumpDown && !jumpHeld) {
            capsule.jump();
        }
        jumpHeld = jumpDown;

        if (auto* blendSpace = std::get_if<Component::BlendSpace1D>(
                &stateMachine.states["IdleWalkRun"])) {
            blendSpace->blendParameter = capsule.moveSpeed / capsule.maxRunSpeed;
        }

        if (Input::buttonLeft.down && stateMachine.currentStateName != "Attack") {
            stateMachine.setCurrentState("Attack");
        }
    }
};

struct NpcController {
    glm::vec3 waypoint{0.0f};
    float idleTimer{0.0f};
    uint32_t rng;
    float preferredPace{0.5f};

    static constexpr float kArrivalDist = 2.0f;
    static constexpr float kWanderMin   = 15.0f;
    static constexpr float kWanderMax   = 60.0f;
    static constexpr float kCityHalf    = 250.0f;

    static constexpr float kRayLength       = 10.0f;
    static constexpr float kSideRayAngle    = 0.50f;
    static constexpr float kWideSideAngle   = 1.05f;
    static constexpr float kAvoidSteerSpeed = 6.0f;
    static constexpr float kStuckTimeout    = 1.5f;
    static constexpr float kBlockedWpTime   = 4.0f;
    float stuckTimer{0.0f};
    float blockedTowardWpTimer{0.0f};

    float nextFloat(float lo, float hi) {
        rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
        float t = (rng & 0xFFFF) / 65535.0f;
        return lo + t * (hi - lo);
    }

    static void updateIdleWalkRunBlend(Component::CharacterCapsule& capsule,
                                       Component::AnimationStateMachine& stateMachine)
    {
        auto* bs = std::get_if<Component::BlendSpace1D>(&stateMachine.states["IdleWalkRun"]);
        if (!bs) return;

        float gaitMax = capsule.isRunning ? capsule.maxRunSpeed : capsule.maxWalkSpeed;
        float gaitNorm = (gaitMax > 0.0f) ? std::clamp(capsule.moveSpeed / gaitMax, 0.0f, 1.0f) : 0.0f;

        bs->blendParameter = capsule.isRunning
            ? (0.5f + 0.5f * gaitNorm)
            : (0.5f * gaitNorm);
    }

    void pickNewWaypoint(const glm::vec3& pos) {
        float angle  = nextFloat(0.0f, 6.2832f);
        float radius = nextFloat(kWanderMin, kWanderMax);
        float wx = pos.x + radius * std::cos(angle);
        float wz = pos.z + radius * std::sin(angle);
        wx = std::clamp(wx, -kCityHalf, kCityHalf);
        wz = std::clamp(wz, -kCityHalf, kCityHalf);
        waypoint = glm::vec3(wx, pos.y, wz);
        blockedTowardWpTimer = 0.0f;
    }

    static float castObstacleRay(const glm::vec3& origin,
                                 const glm::vec3& dir2d,
                                 float maxDist,
                                 JPH::BodyID ownBodyId)
    {
        JPH::Vec3 rayOrigin(origin.x, origin.y, origin.z);
        JPH::Vec3 rayDir(dir2d.x * maxDist, 0.0f, dir2d.z * maxDist);

        JPH::RRayCast ray(rayOrigin, rayDir);
        JPH::RayCastResult result;
        result.mFraction = 1.0f;

        auto& phys = Core::GetPhysics().getPhysicsSystem();
        JPH::IgnoreSingleBodyFilter bodyFilter(ownBodyId);

        class ObstacleLayerFilter final : public JPH::ObjectLayerFilter {
        public:
            bool ShouldCollide(JPH::ObjectLayer inLayer) const override {
                return inLayer == Layers::STATIC_ENVIRONMENT
                    || inLayer == Layers::DYNAMIC_ENVIRONMENT
                    || inLayer == Layers::ENEMY_BODY
                    || inLayer == Layers::PLAYER_BODY;
            }
        };
        ObstacleLayerFilter layerFilter;

        if (phys.GetNarrowPhaseQuery().CastRay(ray, result,
                {}, layerFilter, bodyFilter))
        {
            return result.mFraction;
        }
        return 1.0f;
    }

    static glm::vec3 rotateXZ(const glm::vec3& d, float angle) {
        float c = std::cos(angle), s = std::sin(angle);
        return glm::vec3(d.x * c - d.z * s, 0.0f, d.x * s + d.z * c);
    }

    void update(Component::CharacterCapsule& capsule,
                Component::AnimationStateMachine& stateMachine,
                const Component::Transform& npcTransform)
    {
        if (!capsule.character) return;

        const float dt = Time::deltaTime;

        if (idleTimer > 0.0f) {
            idleTimer -= dt;
            capsule.isRunning = false;
            capsule.decrSpeed();

            JPH::Vec3 fwdJ = capsule.rotation * JPH::Vec3(0,0,1);
            capsule.move(glm::vec3(fwdJ.GetX(), 0, fwdJ.GetZ()));
            updateIdleWalkRunBlend(capsule, stateMachine);

            if (idleTimer <= 0.0f) {
                stuckTimer = 0.0f;
                pickNewWaypoint(npcTransform.position);
            }
            return;
        }

        glm::vec3 dir = waypoint - npcTransform.position;
        dir.y = 0.0f;
        float dist = glm::length(dir);

        if (dist < kArrivalDist) {
            stuckTimer = 0.0f;
            idleTimer = nextFloat(1.0f, 5.0f);
            capsule.isRunning = false;
            capsule.decrSpeed();
            JPH::Vec3 fwdJ = capsule.rotation * JPH::Vec3(0,0,1);
            capsule.move(glm::vec3(fwdJ.GetX(), 0, fwdJ.GetZ()));
            updateIdleWalkRunBlend(capsule, stateMachine);
            return;
        }

        dir = glm::normalize(dir);

        JPH::Vec3 fwdJolt = capsule.rotation * JPH::Vec3(0.0f, 0.0f, 1.0f);
        glm::vec3 fwd(fwdJolt.GetX(), 0.0f, fwdJolt.GetZ());
        if (glm::length(fwd) > 0.0f) fwd = glm::normalize(fwd);

        JPH::BodyID ownBody = capsule.character->GetInnerBodyID();

        float hitC  = castObstacleRay(npcTransform.position, fwd, kRayLength, ownBody);
        float hitL  = castObstacleRay(npcTransform.position, rotateXZ(fwd,  kSideRayAngle), kRayLength, ownBody);
        float hitR  = castObstacleRay(npcTransform.position, rotateXZ(fwd, -kSideRayAngle), kRayLength, ownBody);
        float hitWL = castObstacleRay(npcTransform.position, rotateXZ(fwd,  kWideSideAngle), kRayLength * 0.6f, ownBody);
        float hitWR = castObstacleRay(npcTransform.position, rotateXZ(fwd, -kWideSideAngle), kRayLength * 0.6f, ownBody);

        bool blocked = hitC < 0.9f;

        if (blocked && capsule.moveSpeed < 0.5f) {
            stuckTimer += dt;
        } else {
            stuckTimer = std::max(0.0f, stuckTimer - dt * 2.0f);
        }

        if (blocked) {
            blockedTowardWpTimer += dt;
        } else {
            blockedTowardWpTimer = std::max(0.0f, blockedTowardWpTimer - dt);
        }

        if (stuckTimer > kStuckTimeout || blockedTowardWpTimer > kBlockedWpTime) {
            stuckTimer = 0.0f;
            pickNewWaypoint(npcTransform.position);
            float kickTurn = (hitL > hitR) ? -1.0f : 1.0f;
            capsule.turn(kickTurn * 0.5f);
            capsule.decrSpeed();
            fwdJolt = capsule.rotation * JPH::Vec3(0.0f, 0.0f, 1.0f);
            capsule.move(glm::vec3(fwdJolt.GetX(), 0, fwdJolt.GetZ()));
            updateIdleWalkRunBlend(capsule, stateMachine);
            return;
        }

        float desiredTurn = 0.0f;

        if (blocked) {
            float leftClearance  = hitL  + hitWL;
            float rightClearance = hitR  + hitWR;

            if (leftClearance >= rightClearance) {
                desiredTurn = -kAvoidSteerSpeed * dt;
            } else {
                desiredTurn =  kAvoidSteerSpeed * dt;
            }
        } else {
            float dot = glm::dot(fwd, dir);
            if (dot < 0.99f) {
                glm::vec3 cross = glm::cross(fwd, dir);
                float turnDir = (cross.y < 0.0f) ? -1.0f : 1.0f;
                desiredTurn = turnDir * dt;
            }

            if (hitL  < 0.35f) desiredTurn += 2.0f * dt;
            if (hitR  < 0.35f) desiredTurn -= 2.0f * dt;
            if (hitWL < 0.35f) desiredTurn += 1.0f * dt;
            if (hitWR < 0.35f) desiredTurn -= 1.0f * dt;
        }

        capsule.turn(desiredTurn);

        capsule.isRunning = (preferredPace > 0.7f);
        float desiredSpeed = (capsule.isRunning ? capsule.maxRunSpeed : capsule.maxWalkSpeed) * preferredPace;
        if (hitC < 0.25f) {
            capsule.decrSpeed();
            capsule.decrSpeed();
        } else if (blocked) {
            capsule.decrSpeed();
        } else if (capsule.moveSpeed < desiredSpeed) {
            capsule.incrSpeed();
        } else if (capsule.moveSpeed > desiredSpeed + 0.1f) {
            capsule.decrSpeed();
        }

        fwdJolt = capsule.rotation * JPH::Vec3(0.0f, 0.0f, 1.0f);
        glm::vec3 moveDir(fwdJolt.GetX(), 0.0f, fwdJolt.GetZ());
        if (glm::length(moveDir) > 0.0f) moveDir = glm::normalize(moveDir);
        capsule.move(moveDir);

        updateIdleWalkRunBlend(capsule, stateMachine);
    }
};
