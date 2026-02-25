#pragma once

#include "flecs.h"
namespace Lca {
    namespace Component {
        struct Persistent {};
        struct PersistentSystem{};
    }
    namespace Module {
        struct Persistent {
            Persistent(flecs::world& world) {
                world.component<Lca::Component::Persistent>();
            }
        };
    }
}