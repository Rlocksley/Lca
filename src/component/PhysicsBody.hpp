#pragma once

#include "Global.hpp"
#include "flecs.h"
#include "../core/Physics.hpp"
#include "Persistent.hpp"

namespace Lca {
    namespace Component {
        struct PhysicsBody {
            JPH::BodyID bodyID{0};
        };
    }

    namespace Module {
        struct PhysicsBodyModule {
            PhysicsBodyModule(flecs::world& world){
                world.component<Component::PhysicsBody>();

                // Ensure Jolt body is removed/destroyed when component removed
                world.observer<const Component::PhysicsBody>()
                    .event(flecs::OnRemove)
                    .each([&](flecs::entity e, const Component::PhysicsBody& pb){
                        auto& phys = Core::GetPhysics();
                        if(!phys.isInitialized()) return;
                        phys.removeBody(pb.bodyID);
                        phys.destroyBody(pb.bodyID);
                    })
                    .add<Component::PersistentSystem>();
            }
        };
    }
}
