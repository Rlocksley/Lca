#pragma once

#include "Global.hpp"
#include "flecs.h"

namespace Lca {
    namespace Component {
        struct ContactInfo {
            glm::vec3 point;
            glm::vec3 normal;
        };
    }
}
