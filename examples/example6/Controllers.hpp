#pragma once

#include "AnimationStateMachine.hpp"
#include "CharacterCapsule.hpp"
#include "Input.hpp"
#include "NavMesh.hpp"
#include "Navigation.hpp"
#include "Time.hpp"
#include "Transform.hpp"

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
    float idleTimer{0.0f};
    uint32_t rng;
    float preferredPace{0.5f};

    static constexpr float kWanderMin   = 15.0f;
    static constexpr float kWanderMax   = 60.0f;
    static constexpr float kCityHalf    = 250.0f;

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

    void chooseNewTarget(const glm::vec3& pos,
                         Component::NavAgent& agent,
                         Component::NavTarget& target) {
        float angle  = nextFloat(0.0f, 6.2832f);
        float radius = nextFloat(kWanderMin, kWanderMax);
        float wx = pos.x + radius * std::cos(angle);
        float wz = pos.z + radius * std::sin(angle);
        wx = std::clamp(wx, -kCityHalf, kCityHalf);
        wz = std::clamp(wz, -kCityHalf, kCityHalf);
        // Let findPath() project onto the navmesh — avoids thread-safety
        // issues and redundant queries.
        target.worldPos = glm::vec3(wx, pos.y, wz);
        target.active = true;
        agent.requestRepath = true;
        agent.repathTimer = 0.0f;
    }

    void update(const Component::Transform& npcTransform,
                Component::NavAgent& navAgent,
                Component::NavTarget& navTarget,
                const Component::NavSteering& navSteering,
                Component::CharacterCapsule& capsule,
                Component::AnimationStateMachine& stateMachine,
                bool navAvailable)
    {
        if (!capsule.character) return;

        const float dt = Time::deltaTime;

        // Set nav agent desired speed based on preferred pace — don't touch
        // capsule.isRunning here, the NavMesh system owns that flag.
        const float gaitTop = preferredPace > 0.7f ? capsule.maxRunSpeed : capsule.maxWalkSpeed;
        navAgent.maxSpeed = std::max(0.1f, gaitTop * std::clamp(preferredPace, 0.2f, 1.0f));

        if (idleTimer > 0.0f) {
            idleTimer -= dt;
            navTarget.active = false;
            capsule.decrSpeed();
            updateIdleWalkRunBlend(capsule, stateMachine);

            if (idleTimer <= 0.0f) {
                chooseNewTarget(npcTransform.position, navAgent, navTarget);
            }
            return;
        }

        if (!navAvailable) {
            capsule.decrSpeed();
            updateIdleWalkRunBlend(capsule, stateMachine);
            return;
        }

        if (!navTarget.active) {
            chooseNewTarget(npcTransform.position, navAgent, navTarget);
        }

        if (navSteering.reachedGoal) {
            idleTimer = nextFloat(1.0f, 5.0f);
            navTarget.active = false;
            capsule.decrSpeed();
            updateIdleWalkRunBlend(capsule, stateMachine);
            return;
        }

        updateIdleWalkRunBlend(capsule, stateMachine);
    }
};
