#pragma once

#include "Global.hpp"
#include "Time.hpp"
#include "flecs.h"
#include "Transform.hpp"
#include "Persistent.hpp"
#include "Velocity.hpp"
#include "PhysicsBody.hpp"
#include "../core/Physics.hpp"
#include <Jolt/Physics/Character/CharacterVirtual.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>

namespace Lca {
    namespace Component {
        struct CharacterCapsule {
            float capsuleHeight{1.8f};
            float capsuleRadius{0.3f};
            JPH::ObjectLayer layer{0};
            JPH::CharacterVirtual* character{nullptr};

            float turnSpeed{3.0f};
            float maxWalkSpeed{2.0f};
            float maxRunSpeed{5.0f};
            float minMoveSpeed{0.0f};
            float speedIncrement{6.0f};
            float speedDecrement{8.0f};
            float jumpSpeed{6.0f};

            bool isRunning{false};
            bool startJumping{false};

            JPH::Quat rotation{JPH::Quat::sIdentity()};
            JPH::Vec3 velocity{JPH::Vec3::sZero()};
            JPH::Vec3 gravityVelocity{JPH::Vec3::sZero()};
            float moveSpeed{0.0f};

            void turn(float direction) {
                rotation = rotation * JPH::Quat::sRotation(JPH::Vec3(0.0f, 1.0f, 0.0f), direction * turnSpeed);
            }

            void move(const glm::vec3& direction) {
                velocity = Core::toJoltVec3(direction) * moveSpeed;
            }

            void jump() {
                if (character && character->GetGroundState() == JPH::CharacterVirtual::EGroundState::OnGround) {
                    gravityVelocity += JPH::Vec3(0.0f, jumpSpeed, 0.0f);
                    startJumping = true;
                }
            }

            void incrSpeed() {
                float targetSpeed = isRunning ? maxRunSpeed : maxWalkSpeed;
                moveSpeed = std::min(moveSpeed + speedIncrement * Time::deltaTime, targetSpeed);
            }

            void decrSpeed() {
                float targetSpeed = isRunning ? maxWalkSpeed : minMoveSpeed;
                if (moveSpeed > targetSpeed) {
                    moveSpeed = std::max(moveSpeed - speedDecrement * Time::deltaTime, targetSpeed);
                }
            }

            void update(){
                if(!character) return;

                if(character->GetGroundState() != JPH::CharacterVirtual::EGroundState::OnGround){
                    gravityVelocity += JPH::Vec3(0.0f, -9.81f, 0.0f) * Time::deltaTime;
                } else if(!startJumping){
                    gravityVelocity = JPH::Vec3::sZero();
                } else {
                    startJumping = false;
                }

                character->SetLinearVelocity(velocity + gravityVelocity);
                character->SetRotation(rotation);
            }

            void updateTransform(Component::Transform& t){
                if(character){
                    JPH::Vec3 pos = character->GetPosition();
                    JPH::Quat rot = character->GetRotation();
                    t.position = Core::toGlmVec3(pos);
                    t.rotation = Core::toGlmQuat(rot);
                }
            }
        };
    }

    namespace Module {
        struct CharacterCapsuleModule {
            CharacterCapsuleModule(flecs::world& world){
                world.component<Component::CharacterCapsule>();

                world.observer<Component::Transform, Component::CharacterCapsule>()
                    .event(flecs::OnSet)
                    .each([&](flecs::entity e, Component::Transform& tf, Component::CharacterCapsule& capsule){
                        if(capsule.character) return;
                        auto& phys = Core::GetPhysics();
                        if(!phys.isInitialized()) return;

                        JPH::Ref<JPH::ShapeSettings> shape_settings = new JPH::CapsuleShapeSettings(capsule.capsuleHeight/2.0f, capsule.capsuleRadius);

                        JPH::CharacterVirtualSettings char_settings;
                        char_settings.mMass = 80.0f;
                        char_settings.mMaxSlopeAngle = JPH::DegreesToRadians(50.0f);
                        char_settings.mShape = shape_settings->Create().Get();
                        char_settings.mInnerBodyShape = shape_settings->Create().Get();
                        char_settings.mInnerBodyLayer = capsule.layer;
                        char_settings.mMaxStrength = 100.0f;

                        capsule.character = new JPH::CharacterVirtual(&char_settings, Core::toJoltVec3(tf.position), Core::toJoltQuat(tf.rotation), 0, &phys.getPhysicsSystem());
                        capsule.character->SetUserData(e.id());
                        capsule.rotation = Core::toJoltQuat(tf.rotation);
                        // character contact listener already attached to physics system in Core::GetPhysics().init()
                    })
                    .add<Component::PersistentSystem>();

                world.observer<Component::CharacterCapsule>()
                    .event(flecs::OnRemove)
                    .each([&](flecs::entity e, Component::CharacterCapsule& capsule){
                        if(capsule.character){ delete capsule.character; capsule.character = nullptr; }
                    })
                    .add<Component::PersistentSystem>();

                // Update / sync systems
                world.system<Component::Transform, Component::CharacterCapsule>()
                    //.multi_threaded()
                .each([&](flecs::entity e, Component::Transform& transform, Component::CharacterCapsule& capsule){
                        if(!capsule.character) return;
                        capsule.update();
                        const JPH::BodyFilter body_filter;
                        const JPH::ShapeFilter shape_filter;
                        capsule.character->Update(Time::deltaTime, Core::GetPhysics().getPhysicsSystem().GetGravity(), Core::GetPhysics().getPhysicsSystem().GetDefaultBroadPhaseLayerFilter(capsule.layer), Core::GetPhysics().getPhysicsSystem().GetDefaultLayerFilter(capsule.layer), body_filter, shape_filter, *Core::GetPhysics().getTempAllocator());
                        capsule.updateTransform(transform);
                    })
                    .add<Component::PersistentSystem>();

                world.system<Component::Velocity, Component::CharacterCapsule>()
                    .multi_threaded()
                    .each([&](flecs::entity e, Component::Velocity& vel, Component::CharacterCapsule& capsule){
                        if(!capsule.character) return;
                        vel.velocity = Core::toGlmVec3(capsule.character->GetLinearVelocity());
                        vel.angularVelocity = glm::vec3(0.0f);
                    })
                    .add<Component::PersistentSystem>();
            }
        };
    }
}
