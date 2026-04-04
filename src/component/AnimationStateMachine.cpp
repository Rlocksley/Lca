#include "AnimationStateMachine.hpp"

namespace Lca {
    namespace Component {

        // ── Sampling helpers ───────────────────────────────────

        static uint32_t findUpperKeyIndex(const float* times, uint32_t count, float t) {
            for (uint32_t i = 0; i < count; ++i) {
                if (t <= times[i]) return i;
            }
            return count - 1;
        }

        static glm::vec3 samplePosition(const Core::Animation& anim, const Core::AnimationChannel& ch, float t) {
            if (ch.positionCount == 0) return glm::vec3(0.0f);
            if (ch.positionCount == 1) return anim.positionKeys[ch.positionOffset].value;

            uint32_t upper = 0;
            for (uint32_t i = 0; i < ch.positionCount; ++i) {
                if (t <= anim.positionKeys[ch.positionOffset + i].time) { upper = i; break; }
                upper = i;
            }
            if (upper == 0) return anim.positionKeys[ch.positionOffset].value;

            uint32_t lower = upper - 1;
            const auto& k0 = anim.positionKeys[ch.positionOffset + lower];
            const auto& k1 = anim.positionKeys[ch.positionOffset + upper];
            float denom = k1.time - k0.time;
            float factor = (denom > 1e-6f) ? (t - k0.time) / denom : 0.0f;
            return glm::mix(k0.value, k1.value, factor);
        }

        static glm::quat sampleRotation(const Core::Animation& anim, const Core::AnimationChannel& ch, float t) {
            if (ch.rotationCount == 0) return glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
            if (ch.rotationCount == 1) return anim.rotationKeys[ch.rotationOffset].value;

            uint32_t upper = 0;
            for (uint32_t i = 0; i < ch.rotationCount; ++i) {
                if (t <= anim.rotationKeys[ch.rotationOffset + i].time) { upper = i; break; }
                upper = i;
            }
            if (upper == 0) return anim.rotationKeys[ch.rotationOffset].value;

            uint32_t lower = upper - 1;
            const auto& k0 = anim.rotationKeys[ch.rotationOffset + lower];
            const auto& k1 = anim.rotationKeys[ch.rotationOffset + upper];
            float denom = k1.time - k0.time;
            float factor = (denom > 1e-6f) ? (t - k0.time) / denom : 0.0f;
            return glm::slerp(k0.value, k1.value, factor);
        }

        static glm::vec3 sampleScale(const Core::Animation& anim, const Core::AnimationChannel& ch, float t) {
            if (ch.scaleCount == 0) return glm::vec3(1.0f);
            if (ch.scaleCount == 1) return anim.scaleKeys[ch.scaleOffset].value;

            uint32_t upper = 0;
            for (uint32_t i = 0; i < ch.scaleCount; ++i) {
                if (t <= anim.scaleKeys[ch.scaleOffset + i].time) { upper = i; break; }
                upper = i;
            }
            if (upper == 0) return anim.scaleKeys[ch.scaleOffset].value;

            uint32_t lower = upper - 1;
            const auto& k0 = anim.scaleKeys[ch.scaleOffset + lower];
            const auto& k1 = anim.scaleKeys[ch.scaleOffset + upper];
            float denom = k1.time - k0.time;
            float factor = (denom > 1e-6f) ? (t - k0.time) / denom : 0.0f;
            return glm::mix(k0.value, k1.value, factor);
        }

        // ── AnimationSampler ───────────────────────────────────

        std::vector<Core::NodePose> AnimationSampler::sampleAnimation(
                const Core::Animation& anim, float animationTime) {
            std::vector<Core::NodePose> pose(anim.channels.size());
            for (uint32_t i = 0; i < static_cast<uint32_t>(anim.channels.size()); ++i) {
                const auto& ch = anim.channels[i];
                pose[i].position = samplePosition(anim, ch, animationTime);
                pose[i].rotation = sampleRotation(anim, ch, animationTime);
                pose[i].scale    = sampleScale(anim, ch, animationTime);
            }
            return pose;
        }

        std::vector<Core::NodePose> AnimationSampler::blendPoses(
                const std::vector<Core::NodePose>& a,
                const std::vector<Core::NodePose>& b,
                float t) {
            size_t count = std::min(a.size(), b.size());
            std::vector<Core::NodePose> result(count);
            for (size_t i = 0; i < count; ++i) {
                result[i].position = glm::mix(a[i].position, b[i].position, t);
                result[i].rotation = glm::slerp(a[i].rotation, b[i].rotation, t);
                result[i].scale    = glm::mix(a[i].scale, b[i].scale, t);
            }
            return result;
        }

        // ── AnimationStateMachine ──────────────────────────────

        float AnimationStateMachine::getStateDuration(const AnimationStateVariant& state) {
            return std::visit([](const auto& s) -> float {
                return s.animationDuration;
            }, state);
        }

        void AnimationStateMachine::addState(const std::string& name, const AnimationStateVariant& state) {
            states[name] = state;
        }

        void AnimationStateMachine::setCurrentState(const std::string& name) {
            auto it = states.find(name);
            if (it == states.end()) {
                LCA_LOGE("AnimationStateMachine", "setCurrentState", "State '" + name + "' not found.");
            }
            currentStateName = name;
            if (std::holds_alternative<Transition>(it->second)) {
                std::get<Transition>(it->second).time = 0.0f;
            }
        }

        void AnimationStateMachine::transitionToState(const std::string& toStateName, float duration) {
            if (states.find(toStateName) == states.end()) {
                LCA_LOGE("AnimationStateMachine", "transitionToState", "Destination state '" + toStateName + "' not found.");
                return;
            }
            if (currentStateName == toStateName) return;

            std::string transitionName = currentStateName + "_to_" + toStateName;

            Transition transition;
            transition.fromState = currentStateName;
            transition.toState = toStateName;
            transition.duration = duration;
            transition.time = 0.0f;

            states[transitionName] = transition;
            setCurrentState(transitionName);
        }

        std::vector<Core::NodePose> AnimationStateMachine::update(float deltaTime) {
            auto it = states.find(currentStateName);
            LCA_ASSERT(it != states.end(), "AnimationStateMachine", "update", "Current state '" + currentStateName + "' not found.");
            return evaluateState(it->second, deltaTime);
        }

        std::vector<Core::NodePose> AnimationStateMachine::evaluateState(AnimationStateVariant& state, float deltaTime) {
            return std::visit([this, deltaTime](auto& s) -> std::vector<Core::NodePose> {
                return this->evaluateState(s, deltaTime);
            }, state);
        }

        std::vector<Core::NodePose> AnimationStateMachine::evaluateState(SingleAnimation& state, float deltaTime) {
            const auto& anim = Core::GetAssetManager().getAnimation(state.animationName);

            float tickTime = state.animationTime;
            std::vector<Core::NodePose> pose = AnimationSampler::sampleAnimation(anim, tickTime);

            float tps = anim.ticksPerSecond > 0.0f ? anim.ticksPerSecond : 25.0f;
            state.animationDuration = anim.duration / tps;
            state.animationTime += tps * state.animationSpeed * deltaTime;

            if (state.looping) {
                state.animationTime = std::fmod(state.animationTime, anim.duration);
                if (state.animationTime < 0.0f) state.animationTime += anim.duration;
            } else {
                state.animationTime = std::min(state.animationTime, anim.duration);
            }

            return pose;
        }

        std::vector<Core::NodePose> AnimationStateMachine::evaluateState(BlendSpace1D& state, float deltaTime) {
            state.calculateNearestPoints();
            state.calculateBlendFactor();

            int8_t i0 = state.nearestIndices[0];
            int8_t i1 = state.nearestIndices[1];

            const auto& anim0 = Core::GetAssetManager().getAnimation(state.animationNames[i0]);
            const auto& anim1 = Core::GetAssetManager().getAnimation(state.animationNames[i1]);

            float tps0 = anim0.ticksPerSecond > 0.0f ? anim0.ticksPerSecond : 25.0f;
            float tps1 = anim1.ticksPerSecond > 0.0f ? anim1.ticksPerSecond : 25.0f;

            // Normalize time to [0,1] for synchronized blend
            float time0 = state.animationTime * anim0.duration;
            float time1 = state.animationTime * anim1.duration;

            auto pose0 = AnimationSampler::sampleAnimation(anim0, time0);
            auto pose1 = AnimationSampler::sampleAnimation(anim1, time1);
            auto blended = AnimationSampler::blendPoses(pose0, pose1, state.blendFactor);

            // Compute raw animation duration (not speed-adjusted)
            float rawDur0 = anim0.duration / tps0;
            float rawDur1 = anim1.duration / tps1;
            state.animationDuration = rawDur0 * (1.0f - state.blendFactor) + rawDur1 * state.blendFactor;

            // Advance normalized time
            float dur0Sec = anim0.duration / (tps0 * state.animationSpeed[i0]);
            float dur1Sec = anim1.duration / (tps1 * state.animationSpeed[i1]);
            float blendedDur = dur0Sec * (1.0f - state.blendFactor) + dur1Sec * state.blendFactor;
            state.animationTime = std::fmod(state.animationTime + deltaTime / blendedDur, 1.0f);

            return blended;
        }

        std::vector<Core::NodePose> AnimationStateMachine::evaluateState(Transition& state, float deltaTime) {
            auto& fromVariant = states.at(state.fromState);
            auto& toVariant   = states.at(state.toState);

            auto poseFrom = evaluateState(fromVariant, deltaTime);
            auto poseTo   = evaluateState(toVariant, deltaTime);

            float blendFactor = std::min(1.0f, state.time / state.duration);
            auto blended = AnimationSampler::blendPoses(poseFrom, poseTo, blendFactor);

            state.animationDuration = getStateDuration(fromVariant) * (1.0f - blendFactor)
                                    + getStateDuration(toVariant) * blendFactor;

            state.time += deltaTime;

            if (state.time >= state.duration) {
                setCurrentState(state.toState);
            }

            return blended;
        }

        std::vector<Core::NodePose> AnimationStateMachine::evaluateState(ActionAnimationBlend& state, float deltaTime) {
            auto& actionVariant = states.at(state.actionState);
            auto& baseVariant   = states.at(state.baseState);

            // Evaluate sub-states first (populates animationDuration)
            auto actionPose = evaluateState(actionVariant, deltaTime);
            auto basePose   = evaluateState(baseVariant, deltaTime);

            // Get action duration generically from any state type
            float actionDuration = getStateDuration(actionVariant);
            state.animationDuration = actionDuration;

            // Blend factor: ramp in, hold, ramp out
            float blendFactor = 1.0f;
            if (state.time < state.blendInDuration) {
                blendFactor = state.time / state.blendInDuration;
            } else if (actionDuration > 0.0f && state.time > actionDuration - state.blendOutDuration) {
                float timeInBlendOut = state.time - (actionDuration - state.blendOutDuration);
                blendFactor = 1.0f - (timeInBlendOut / state.blendOutDuration);
            }
            blendFactor = glm::clamp(blendFactor, 0.0f, 1.0f);

            // Start from base, overwrite only masked nodes
            auto finalPose = basePose;
            for (uint32_t nodeIdx : state.nodeMask) {
                if (nodeIdx < finalPose.size() && nodeIdx < actionPose.size()) {
                    finalPose[nodeIdx].position = glm::mix(basePose[nodeIdx].position, actionPose[nodeIdx].position, blendFactor);
                    finalPose[nodeIdx].rotation = glm::slerp(basePose[nodeIdx].rotation, actionPose[nodeIdx].rotation, blendFactor);
                    finalPose[nodeIdx].scale    = glm::mix(basePose[nodeIdx].scale, actionPose[nodeIdx].scale, blendFactor);
                }
            }

            state.time += deltaTime;

            // When action finishes, return to base state
            if (auto* single = std::get_if<SingleAnimation>(&actionVariant)) {
                if (!single->looping && single->animationTime >= Core::GetAssetManager().getAnimation(single->animationName).duration) {
                    single->animationTime = 0.0f;
                    setCurrentState(state.baseState);
                    state.time = 0.0f;
                }
            }

            return finalPose;
        }

        std::vector<Core::NodePose> AnimationStateMachine::evaluateState(MaskedBlend& state, float deltaTime) {
            auto& maskedVariant        = states.at(state.maskedState);
            auto& complementaryVariant = states.at(state.complementaryState);

            auto maskedPose        = evaluateState(maskedVariant, deltaTime);
            auto complementaryPose = evaluateState(complementaryVariant, deltaTime);

            state.animationDuration = getStateDuration(maskedVariant);

            // Build a lookup set for masked nodes
            size_t poseSize = std::max(maskedPose.size(), complementaryPose.size());
            std::vector<bool> isMasked(poseSize, false);
            for (uint32_t nodeIdx : state.nodeMask) {
                if (nodeIdx < poseSize) isMasked[nodeIdx] = true;
            }

            // Compose final pose: masked nodes from maskedState, rest from complementaryState
            std::vector<Core::NodePose> finalPose(poseSize);
            for (size_t i = 0; i < poseSize; ++i) {
                if (isMasked[i] && i < maskedPose.size()) {
                    finalPose[i] = maskedPose[i];
                } else if (i < complementaryPose.size()) {
                    finalPose[i] = complementaryPose[i];
                }
            }

            return finalPose;
        }

        // ── Bone matrix computation ────────────────────────────

        void computeBoneMatrices(
            const std::string& skeletonName,
            const std::vector<Core::NodePose>& nodePoses,
            Core::SkeletonInstance& outInstance)
        {
            const auto& skelData = Core::GetAssetManager().getSkeletonData(skeletonName);
            const uint32_t nodeCount = skelData.nodeCount;

            // Compute global transform for each node
            std::vector<glm::mat4> globalTransforms(nodeCount, glm::mat4(1.0f));

            for (uint32_t i = 0; i < nodeCount; ++i) {
                const auto& node = skelData.nodes[i];

                glm::mat4 localTransform;
                if (i < nodePoses.size()) {
                    // Build local transform from animation pose
                    glm::mat4 T = glm::translate(glm::mat4(1.0f), nodePoses[i].position);
                    glm::mat4 R = glm::toMat4(nodePoses[i].rotation);
                    glm::mat4 S = glm::scale(glm::mat4(1.0f), nodePoses[i].scale);
                    localTransform = T * R * S;
                } else {
                    // No animation data for this node — use stored default
                    if (!node.isBone) {
                        localTransform = node.offsetMatrix;
                    } else {
                        localTransform = glm::mat4(1.0f);
                    }
                }

                if (node.parentIndex >= 0) {
                    globalTransforms[i] = globalTransforms[node.parentIndex] * localTransform;
                } else {
                    globalTransforms[i] = localTransform;
                }
            }

            // Compute final bone matrices
            for (uint32_t i = 0; i < Core::MAX_BONES_PER_SKELETON; ++i) {
                if (i < nodeCount) {
                    outInstance.boneTransform[i] = globalTransforms[i];
                    if (skelData.nodes[i].isBone) {
                        outInstance.boneWithInvBindMatrices[i] = globalTransforms[i] * skelData.nodes[i].offsetMatrix;
                    } else {
                        outInstance.boneWithInvBindMatrices[i] = glm::mat4(1.0f);
                    }
                } else {
                    outInstance.boneWithInvBindMatrices[i] = glm::mat4(1.0f);
                    outInstance.boneTransform[i] = glm::mat4(1.0f);
                }
            }
        }
    }

    namespace Module {
        AnimationStateMachine::AnimationStateMachine(flecs::world& world) {
            world.component<Component::AnimationStateMachine>();

            // System: runs on parent entities that have AnimationStateMachine + SkeletonInstanceID.
            // Evaluates the state machine, computes bone matrices, and writes them
            // to the single SkeletonInstance owned by this parent.
            world.system<Component::AnimationStateMachine, const Component::SkeletonInstanceID>("AnimationStateMachineUpdate")
                .multi_threaded()
                .each([](flecs::entity e, Component::AnimationStateMachine& asm_, const Component::SkeletonInstanceID& skelInstID) {
                    //if (skelInstID.id == UINT32_MAX) return;

                    // Update the animation state machine and get node poses
                    std::vector<Core::NodePose> nodePoses = asm_.update(Time::deltaTime);

                    // Compute bone matrices from node poses and skeleton hierarchy
                    Core::SkeletonInstance skelInstance{};
                    Component::computeBoneMatrices(asm_.skeletonName, nodePoses, skelInstance);

                    // Write bone matrices to renderer (current frame only)
                    Core::GetRenderer().updateSkeletonInstance(Core::currentFrameIndex, skelInstID.id, skelInstance);
                })
                .add<Component::PersistentSystem>();
        }
    }
}
