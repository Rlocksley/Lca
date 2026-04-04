#pragma once

#include "Global.hpp"
#include "flecs.h"
#include "Persistent.hpp"
#include "BoxCollider.hpp"
#include "PhysicsBody.hpp"
#include "Transform.hpp"
#include "Velocity.hpp"
#include "../core/Physics.hpp"
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>

namespace Lca {
    namespace Component {
        struct StaticSensor { JPH::ObjectLayer objectLayer{0}; };
        struct KinematicSensor { JPH::ObjectLayer objectLayer{0}; };
    }

    namespace Module {
        struct SensorModule {
            SensorModule(flecs::world& world){
                world.component<Component::StaticSensor>();
                world.component<Component::KinematicSensor>();

                // Static sensor creation
                world.observer<Component::StaticSensor, Component::BoxCollider, Component::Transform>()
                    .event(flecs::OnSet)
                    .each([&](flecs::entity e, Component::StaticSensor& sensor, Component::BoxCollider& bc, Component::Transform& tf){
                        if(e.has<Component::PhysicsBody>()) return;
                        auto& phys = Core::GetPhysics();
                        if(!phys.isInitialized()) return;
                        JPH::Ref<JPH::Shape> boxShape = new JPH::BoxShape(Core::toJoltVec3(bc.halfExtent));
                        JPH::BodyCreationSettings settings(boxShape, Core::toJoltVec3(tf.position), Core::toJoltQuat(tf.rotation), JPH::EMotionType::Static, sensor.objectLayer);
                        settings.mIsSensor = true;
                        JPH::BodyID id = phys.createBody(settings);
                        phys.addBody(id);
                        phys.setUserData(id, e.id());
                        e.set<Component::PhysicsBody>({ id });
                    })
                    .add<Component::PersistentSystem>();

                // Kinematic sensor creation
                world.observer<Component::KinematicSensor, Component::BoxCollider, Component::Transform, Component::Velocity>()
                    .event(flecs::OnSet)
                    .each([&](flecs::entity e, Component::KinematicSensor& sensor, Component::BoxCollider& bc, Component::Transform& tf, Component::Velocity& vel){
                        if(e.has<Component::PhysicsBody>()) return;
                        auto& phys = Core::GetPhysics();
                        if(!phys.isInitialized()) return;
                        JPH::Ref<JPH::Shape> boxShape = new JPH::BoxShape(Core::toJoltVec3(bc.halfExtent));
                        JPH::BodyCreationSettings settings(boxShape, Core::toJoltVec3(tf.position), Core::toJoltQuat(tf.rotation), JPH::EMotionType::Kinematic, sensor.objectLayer);
                        settings.mIsSensor = true;
                        settings.mCollideKinematicVsNonDynamic = true;
                        settings.mLinearVelocity = Core::toJoltVec3(vel.velocity);
                        settings.mAngularVelocity = Core::toJoltVec3(vel.angularVelocity);
                        JPH::BodyID id = phys.createBody(settings);
                        phys.addBody(id);
                        phys.setUserData(id, e.id());
                        e.set<Component::PhysicsBody>({ id });
                    })
                    .add<Component::PersistentSystem>();
            }
        };
    }
}
