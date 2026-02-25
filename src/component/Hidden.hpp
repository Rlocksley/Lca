#pragma once

#include "flecs.h"

namespace Lca{
    namespace Component{
        struct Hidden {};
    }

    namespace Module{
        struct Hidden{
            Hidden(flecs::world& world){
                world.component<Lca::Component::Hidden>();
            }
        };
    }
}
