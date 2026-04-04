#pragma once

#include "Transform.hpp"

namespace Lca {
    namespace Component {
        struct MeshTransform {
            Transform local;
        };
    }

    namespace Module {
        struct MeshTransform {
            MeshTransform(flecs::world& world) {
                world.component<Lca::Component::MeshTransform>();
            }
        };
    }
}
