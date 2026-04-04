#pragma once

#include "Global.hpp"
#include "flecs.h"

namespace Lca {
    namespace Component {
        struct Velocity {
            glm::vec3 velocity{0.0f};
            glm::vec3 angularVelocity{0.0f};
        };
    }
}
