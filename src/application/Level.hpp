#pragma once

#include "flecs.h"
#include <memory>

namespace Lca {

// Tag for entities that survive level loads (e.g., the player)
struct Persistent {};

class Level {
public:
    virtual ~Level() = default;

    // 1. Called on background thread. Load textures, meshes, materials, etc.
    virtual void loadAssets() {}

    // 2. Called on main thread. Create entities, setup scene.
    virtual void setupScene(flecs::world& world) {}

    // 3. Called on main thread before switching levels.
    // Default behavior: delete everything without the Persistent tag.
    virtual void cleanupScene(flecs::world& world) {
        // Delete all entities that do not have the Persistent tag
        world.defer_begin();
        auto q = world.query_builder<>().without<Persistent>().build();
        q.each([](flecs::entity e) {
            e.destruct();
        });
        world.defer_end();
    }
};

// Component to request a level load from within a Flecs system
struct LoadLevelRequest {
    std::shared_ptr<Level> nextLevel;
};

} // namespace Lca
