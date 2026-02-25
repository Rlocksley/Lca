#pragma once

#include "flecs.h"
#include <memory>
#include <vector>
#include <cstring>
#include "Persistent.hpp"

namespace Lca {

class Level {
public:
    virtual ~Level() = default;

    // 1. Called on background thread. Load textures, meshes, materials, etc.
    virtual void loadAssets() {}

    // 2. Called on main thread. Create entities, setup scene.
    virtual void setupScene(flecs::world& world) {}

    // 3. Called on main thread before switching levels.
    //    Default: deletes all non-persistent user entities.
    virtual void cleanupScene(flecs::world& world) {
        deleteNonPersistentEntities(world);
    }

    // Safely delete all user entities without Persistent/PersistentSystem tags.
    // Skips flecs built-in entities, module entities, component definitions,
    // and module-owned systems (stats, REST, etc.).
    static void deleteNonPersistentEntities(flecs::world& world) {
        auto isUserEntity = [](flecs::entity e) {
            ecs_world_t* raw = e.world().c_ptr();
            ecs_entity_t id = e.id();

            if (id < EcsFirstUserEntityId) return false;

            char* path = ecs_get_path_w_sep(raw, 0, id, ".", nullptr);
            const bool isFlecsEntity = path && std::strncmp(path, "flecs", 5) == 0;
            if (path) {
                ecs_os_free(path);
            }
            if (isFlecsEntity) return false;

            if (ecs_has_id(raw, id, ecs_pair(EcsOnDelete, EcsPanic))) return false;

            // Skip module entities — their children (observers/systems) would
            // be cascade-deleted even if tagged PersistentSystem.
            if (ecs_has_id(raw, id, EcsModule)) return false;

            ecs_table_t* table = ecs_get_table(raw, id);
            if (table && ecs_table_has_flags(table, EcsTableHasBuiltins)) return false;

            return true;
        };

        std::vector<flecs::entity> toDelete;

        auto q = world.query_builder<>()
            .without<Lca::Component::Persistent>()
            .without<Lca::Component::PersistentSystem>()
            .build();

        q.each([&toDelete, &isUserEntity](flecs::entity e) {
            if (!isUserEntity(e)) return;
            toDelete.push_back(e);
        });

        // Delete in immediate mode (NOT deferred) to avoid bulk constraint violations
        for (auto& ent : toDelete) {
            if (ent.is_alive()) {
                ent.destruct();
            }
        }
    }
};

// Component to request a level load from within a Flecs system
struct LoadLevelRequest {
    std::shared_ptr<Level> nextLevel;
};

} // namespace Lca
