#pragma once

#include "Global.hpp"
#include "flecs.h"
#include "Persistent.hpp"

namespace Lca {
    namespace Component {
        struct BoxCollider {
            glm::vec3 halfExtent{0.5f,0.5f,0.5f};
        };
    }

    namespace Module {
        struct BoxColliderModule {
            BoxColliderModule(flecs::world& world){
                world.component<Component::BoxCollider>();
            }
        };
    }
}
