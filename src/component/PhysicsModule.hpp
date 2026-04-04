#pragma once

#include "Global.hpp"
#include "flecs.h"
#include "Persistent.hpp"
#include "Time.hpp"
#include "ContactInfo.hpp"
#include "../core/Physics.hpp"

namespace Lca {
    namespace Module {
        struct PhysicsModule {
            PhysicsModule(flecs::world& world) {
                // ensure physics initialized
                Core::GetPhysics().init();

                // create ordering entity for physics update
                auto onPhysicsUpdate = world.entity("onPhysicsUpdate");
                onPhysicsUpdate.depends_on(flecs::PreUpdate);
                world.entity(flecs::OnUpdate).depends_on(onPhysicsUpdate);

                // Physics step system (runs in onPhysicsUpdate)
                world.system("PhysicsUpdate")
                    .kind(onPhysicsUpdate)
                    .run([&](flecs::iter& it){
                        while(it.next()){
                            Core::GetPhysics().step(Time::deltaTime);
                        }
                    })
                    .add<Component::PersistentSystem>();

                // Process collision queue and attach ContactInfo relationships
                world.system("ProcessCollisionEvents")
                    .kind(flecs::PreUpdate)
                    .run([&](flecs::iter& it){
                        while(it.next()){
                            std::vector<Core::CollisionEvent> events;
                            Core::collisionQueue.drain(events);
                            if(events.empty()) return;
                            for(const auto &ev : events){
                                flecs::entity e1(it.world(), ev.entity1);
                                flecs::entity e2(it.world(), ev.entity2);
                                if(e1.is_valid() && e2.is_valid()){
                                    e1.set<Component::ContactInfo>(e2, {ev.contactPoint, ev.contactNormal});
                                    e2.set<Component::ContactInfo>(e1, {ev.contactPoint, -ev.contactNormal});
                                }
                            }
                        }
                    })
                    .add<Component::PersistentSystem>();

                // Cleanup collisions each frame
                world.system("CleanupCollisions")
                    .kind(flecs::PostFrame)
                    .multi_threaded(false)
                    .run([](flecs::iter& it){
                        while(it.next()){
                            it.world().remove_all<Component::ContactInfo>();
                        }
                    })
                    .add<Component::PersistentSystem>();
            }
        };
    }
}
