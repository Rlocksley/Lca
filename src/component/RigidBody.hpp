#pragma once

#include "Global.hpp"
#include "flecs.h"
#include "Persistent.hpp"
#include "Transform.hpp"
#include "Velocity.hpp"
#include "BoxCollider.hpp"
#include "PhysicsBody.hpp"
#include "../core/Physics.hpp"
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>

namespace Lca {
    namespace Component {
        struct StaticRigidBody{ JPH::ObjectLayer objectLayer{0}; float friction{0.5f}; float restitution{0.0f}; };
        struct DynamicRigidBody{ JPH::ObjectLayer objectLayer{0}; float mass{1.0f}; float friction{0.5f}; float restitution{0.0f}; };
        struct KinematicRigidBody{ JPH::ObjectLayer objectLayer{0}; float friction{0.5f}; float restitution{0.0f}; };
    }

    namespace Module {
        struct RigidBodyModule {
            RigidBodyModule(flecs::world& world){
                world.component<Component::StaticRigidBody>();
                world.component<Component::DynamicRigidBody>();
                world.component<Component::KinematicRigidBody>();

                auto& phys = Core::GetPhysics();

                // Static rigid body creation
                world.observer<Component::StaticRigidBody, Component::BoxCollider, Component::Transform>()
                    .event(flecs::OnSet)
                    .each([&](flecs::entity e, Component::StaticRigidBody& rb, Component::BoxCollider& bc, Component::Transform& tf){
                        if(e.has<Component::PhysicsBody>()) return;
                        if(!phys.isInitialized()) return;
                        JPH::BodyCreationSettings settings(new JPH::BoxShape(Core::toJoltVec3(bc.halfExtent)), Core::toJoltVec3(tf.position), Core::toJoltQuat(tf.rotation), JPH::EMotionType::Static, rb.objectLayer);
                        settings.mFriction = rb.friction;
                        settings.mRestitution = rb.restitution;
                        JPH::BodyID id = phys.createBody(settings);
                        phys.addBody(id);
                        phys.setUserData(id, e.id());
                        e.set<Component::PhysicsBody>({ id });
                    })
                    .add<Component::PersistentSystem>();

                // Dynamic rigid body creation
                world.observer<Component::DynamicRigidBody, Component::BoxCollider, Component::Transform, Component::Velocity>()
                    .event(flecs::OnSet)
                    .each([&](flecs::entity e, Component::DynamicRigidBody& rb, Component::BoxCollider& bc, Component::Transform& tf, Component::Velocity& vel){
                        if(e.has<Component::PhysicsBody>()) return;
                        if(!phys.isInitialized()) return;
                        JPH::Ref<JPH::Shape> boxShape = new JPH::BoxShape(Core::toJoltVec3(bc.halfExtent));
                        JPH::BodyCreationSettings settings(boxShape, Core::toJoltVec3(tf.position), Core::toJoltQuat(tf.rotation), JPH::EMotionType::Dynamic, rb.objectLayer);
                        settings.mFriction = rb.friction;
                        settings.mRestitution = rb.restitution;
                        if(rb.mass > 0.0f){
                            settings.mOverrideMassProperties = JPH::EOverrideMassProperties::MassAndInertiaProvided;
                            settings.mMassPropertiesOverride = boxShape->GetMassProperties();
                            settings.mMassPropertiesOverride.ScaleToMass(rb.mass);
                        }
                        settings.mLinearVelocity = Core::toJoltVec3(vel.velocity);
                        settings.mAngularVelocity = Core::toJoltVec3(vel.angularVelocity);
                        JPH::BodyID id = phys.createBody(settings);
                        phys.addBody(id);
                        phys.setUserData(id, e.id());
                        e.set<Component::PhysicsBody>({ id });
                    })
                    .add<Component::PersistentSystem>();

                // Kinematic rigid body creation
                world.observer<Component::KinematicRigidBody, Component::BoxCollider, Component::Transform>()
                    .event(flecs::OnSet)
                    .each([&](flecs::entity e, Component::KinematicRigidBody& rb, Component::BoxCollider& bc, Component::Transform& tf){
                        if(e.has<Component::PhysicsBody>()) return;
                        if(!phys.isInitialized()) return;
                        JPH::Ref<JPH::Shape> boxShape = new JPH::BoxShape(Core::toJoltVec3(bc.halfExtent));
                        JPH::BodyCreationSettings settings(boxShape, Core::toJoltVec3(tf.position), Core::toJoltQuat(tf.rotation), JPH::EMotionType::Kinematic, rb.objectLayer);
                        settings.mFriction = rb.friction;
                        settings.mRestitution = rb.restitution;
                        JPH::BodyID id = phys.createBody(settings);
                        phys.addBody(id);
                        phys.setUserData(id, e.id());
                        e.set<Component::PhysicsBody>({ id });
                    })
                    .add<Component::PersistentSystem>();

                // Sync dynamic bodies back to flecs
                // We'll use an "onPhysicsUpdate" ordering entity created by PhysicsModule
                auto onPhysicsUpdate = world.entity("onPhysicsUpdate");

                world.system<Component::Transform, Component::Velocity, Component::PhysicsBody>("SyncFlecsFromJolt")
                    .with<Component::DynamicRigidBody>()
                    .kind(onPhysicsUpdate)
                    .each([&](flecs::entity e, Component::Transform& tf, Component::Velocity& vel, Component::PhysicsBody& pb){
                        if(!phys.isInitialized()) return;
                        JPH::Vec3 position; JPH::Quat rotation;
                        phys.getBodyInterface().GetPositionAndRotation(pb.bodyID, position, rotation);
                        tf.position = Core::toGlmVec3(position);
                        tf.rotation = Core::toGlmQuat(rotation);
                        JPH::Vec3 lin, ang; phys.getBodyInterface().GetLinearAndAngularVelocity(pb.bodyID, lin, ang);
                        vel.velocity = Core::toGlmVec3(lin);
                        vel.angularVelocity = Core::toGlmVec3(ang);
                    })
                    .add<Component::PersistentSystem>();

                // Sync kinematic bodies' transforms from Jolt
                world.system<Component::Transform, Component::PhysicsBody>()
                    .with<Component::KinematicRigidBody>()
                    .kind(onPhysicsUpdate)
                    .each([&](flecs::entity e, Component::Transform& tf, Component::PhysicsBody& pb){
                        if(!phys.isInitialized()) return;
                        JPH::Vec3 position; JPH::Quat rotation;
                        phys.getBodyInterface().GetPositionAndRotation(pb.bodyID, position, rotation);
                        tf.position = Core::toGlmVec3(position);
                        tf.rotation = Core::toGlmQuat(rotation);
                    })
                    .add<Component::PersistentSystem>();

                // Push velocities from Velocity component to kinematic bodies
                world.system<Component::Velocity, Component::PhysicsBody>()
                    .with<Component::KinematicRigidBody>()
                    .kind(onPhysicsUpdate)
                    .each([&](flecs::entity e, Component::Velocity& vel, Component::PhysicsBody& pb){
                        if(!phys.isInitialized()) return;
                        phys.getBodyInterface().SetLinearAndAngularVelocity(pb.bodyID, Core::toJoltVec3(vel.velocity), Core::toJoltVec3(vel.angularVelocity));
                    })
                    .add<Component::PersistentSystem>();

            }
        };
    }
}
