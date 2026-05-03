#pragma once

#include "Global.hpp"

#include <recastnavigation/DetourNavMesh.h>
#include <recastnavigation/DetourNavMeshQuery.h>

namespace Lca {
namespace Core {

class Navigation {
public:
    Navigation() = default;
    ~Navigation() { shutdown(); }

    // Initializes query resources. Safe to call multiple times.
    bool init(int maxQueryNodes = 4096);

    // Frees navmesh and query resources.
    void shutdown();

    bool isInitialized() const { return navQuery != nullptr; }

    // Set an externally-created navmesh.
    // If takeOwnership is true, Navigation will free it in shutdown/clearNavMesh.
    bool setNavMesh(dtNavMesh* mesh, bool takeOwnership = false);

    // Convenience: initialize a navmesh and add one tile blob.
    // tileFlags can be DT_TILE_FREE_DATA if Detour should own/free tileData.
    bool loadSingleTile(const dtNavMeshParams& params, unsigned char* tileData, int tileDataSize, int tileFlags = DT_TILE_FREE_DATA);

    // Clears current navmesh and detaches it from query.
    void clearNavMesh();

    dtNavMesh* getNavMesh() { return navMesh; }
    const dtNavMesh* getNavMesh() const { return navMesh; }

    dtNavMeshQuery* getNavQuery() { return navQuery; }
    const dtNavMeshQuery* getNavQuery() const { return navQuery; }

    // Finds the nearest point on navmesh to a world-space point.
    bool projectPointToNav(
        const glm::vec3& point,
        const glm::vec3& halfExtents,
        glm::vec3& outPoint,
        dtPolyRef* outRef = nullptr,
        const dtQueryFilter* filter = nullptr
    ) const;

    // Finds a straight path between two world points on the navmesh.
    bool findPath(
        const glm::vec3& start,
        const glm::vec3& end,
        std::vector<glm::vec3>& outStraightPath,
        const glm::vec3& halfExtents = glm::vec3(2.0f, 4.0f, 2.0f),
        const dtQueryFilter* filter = nullptr,
        int maxPolys = 256,
        int maxStraightPoints = 256
    ) const;

    // Detour navmesh raycast (not physics raycast).
    // outHit=true means blocked before end; outHitPos is the hit/end position on navmesh.
    bool raycastPath(
        const glm::vec3& start,
        const glm::vec3& end,
        glm::vec3& outHitPos,
        bool& outHit,
        const glm::vec3& halfExtents = glm::vec3(2.0f, 4.0f, 2.0f),
        const dtQueryFilter* filter = nullptr,
        int maxVisited = 256
    ) const;

private:
    static void toDetour(const glm::vec3& v, float out[3]);
    static glm::vec3 fromDetour(const float in[3]);

    dtNavMesh* navMesh{nullptr};
    dtNavMeshQuery* navQuery{nullptr};
    bool ownsNavMesh{false};
    int queryNodeLimit{4096};

    dtQueryFilter defaultFilter{};
};

inline Navigation& GetNavigation() {
    static Navigation instance;
    return instance;
}

} // namespace Core
} // namespace Lca
