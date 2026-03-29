#pragma once

#include "Global.hpp"
#include "flecs.h"
#include "AssetManager.hpp"
#include "Time.hpp"
#include "Persistent.hpp"
#include "Renderer.hpp"
#include "SkeletonMesh.hpp"
#include <variant>
#include <map>

namespace Lca {
    namespace Component {

        // ── Animation state types ──────────────────────────────

        struct SingleAnimation {
            std::string animationName;
            float animationTime = 0.0f;
            float animationSpeed = 1.0f;
            bool looping = true;
            float animationDuration = 0.0f;
        };

        struct BlendSpace1D {
            static constexpr uint32_t MAX_POINTS = 5;

            std::string animationNames[MAX_POINTS];
            uint32_t pointCount = 0;
            float animationPoints[MAX_POINTS] = {};
            float blendParameter = 0.0f;

            float animationTime = 0.0f;
            float animationSpeed[MAX_POINTS] = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f};

            // Computed each frame
            int8_t nearestIndices[2] = {0, 0};
            float blendFactor = 0.0f;
            float animationDuration = 0.0f;

            void calculateNearestPoints() {
                float minDist1 = std::numeric_limits<float>::max();
                float minDist2 = std::numeric_limits<float>::max();
                int idx1 = 0, idx2 = 0;

                for (uint32_t i = 0; i < pointCount; ++i) {
                    float dist = std::abs(animationPoints[i] - blendParameter);
                    if (dist < minDist1) {
                        minDist2 = minDist1;
                        idx2 = idx1;
                        minDist1 = dist;
                        idx1 = static_cast<int>(i);
                    } else if (dist < minDist2) {
                        minDist2 = dist;
                        idx2 = static_cast<int>(i);
                    }
                }
                if (idx1 > idx2) std::swap(idx1, idx2);
                nearestIndices[0] = static_cast<int8_t>(idx1);
                nearestIndices[1] = static_cast<int8_t>(idx2);
            }

            void calculateBlendFactor() {
                float p0 = animationPoints[nearestIndices[0]];
                float p1 = animationPoints[nearestIndices[1]];
                if (std::abs(p1 - p0) > 1e-5f) {
                    blendFactor = std::abs(blendParameter - p0) / std::abs(p1 - p0);
                } else {
                    blendFactor = 0.0f;
                }
            }
        };

        struct Transition {
            std::string fromState;
            std::string toState;
            float time = 0.0f;
            float duration = 0.25f;
            float animationDuration = 0.0f;
        };

        struct ActionAnimationBlend {
            std::string actionState;
            std::string baseState;
            std::vector<uint32_t> nodeMask;
            float time = 0.0f;
            float blendInDuration = 0.25f;
            float blendOutDuration = 0.25f;
            float animationDuration = 0.0f;
        };

        struct MaskedBlend {
            std::string maskedState;         // animates the masked bones
            std::string complementaryState;  // animates all other bones
            std::vector<uint32_t> nodeMask;
            float animationDuration = 0.0f;
        };

        using AnimationStateVariant = std::variant<
            SingleAnimation,
            BlendSpace1D,
            Transition,
            ActionAnimationBlend,
            MaskedBlend
        >;

        // ── Pose sampling ──────────────────────────────────────

        struct AnimationSampler {
            // Sample an extracted animation at a given time, producing one NodePose per channel
            static std::vector<Core::NodePose> sampleAnimation(
                const Core::Animation& anim, float animationTime);

            // Blend two poses by factor t ∈ [0,1]
            static std::vector<Core::NodePose> blendPoses(
                const std::vector<Core::NodePose>& a,
                const std::vector<Core::NodePose>& b,
                float t);
        };

        // ── State machine ──────────────────────────────────────

        struct AnimationStateMachine {
            std::string skeletonName;
            std::string currentStateName;
            std::map<std::string, AnimationStateVariant> states;

            static float getStateDuration(const AnimationStateVariant& state);

            void addState(const std::string& name, const AnimationStateVariant& state);
            void setCurrentState(const std::string& name);
            void transitionToState(const std::string& toStateName, float duration = 0.25f);

            // Evaluate the current state and return the pose
            std::vector<Core::NodePose> update(float deltaTime);

        private:
            std::vector<Core::NodePose> evaluateState(AnimationStateVariant& state, float deltaTime);
            std::vector<Core::NodePose> evaluateState(SingleAnimation& state, float deltaTime);
            std::vector<Core::NodePose> evaluateState(BlendSpace1D& state, float deltaTime);
            std::vector<Core::NodePose> evaluateState(Transition& state, float deltaTime);
            std::vector<Core::NodePose> evaluateState(ActionAnimationBlend& state, float deltaTime);
            std::vector<Core::NodePose> evaluateState(MaskedBlend& state, float deltaTime);
        };

        // ── Bone matrix computation ────────────────────────────

        // Computes final bone matrices from the AnimationStateMachine's node poses
        // and the skeleton hierarchy. Writes directly into a SkeletonInstance.
        void computeBoneMatrices(
            const std::string& skeletonName,
            const std::vector<Core::NodePose>& nodePoses,
            Core::SkeletonInstance& outInstance);
    }

    namespace Module {
        struct AnimationStateMachine {
            AnimationStateMachine(flecs::world& world);
        };
    }
}
