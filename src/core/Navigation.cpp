#include "Navigation.hpp"

#include <DetourCommon.h>

namespace Lca {
namespace Core {

bool Navigation::init(int maxQueryNodes) {
    if (maxQueryNodes <= 0) {
        std::cerr << "Navigation::init failed: maxQueryNodes must be > 0\n";
        return false;
    }

    queryNodeLimit = maxQueryNodes;

    if (!navQuery) {
        navQuery = dtAllocNavMeshQuery();
        if (!navQuery) {
            std::cerr << "Navigation::init failed: dtAllocNavMeshQuery returned null\n";
            return false;
        }
    }

    if (navMesh) {
        const dtStatus status = navQuery->init(navMesh, queryNodeLimit);
        if (dtStatusFailed(status)) {
            std::cerr << "Navigation::init failed: dtNavMeshQuery::init failed with status " << status << "\n";
            return false;
        }
    }

    return true;
}

void Navigation::shutdown() {
    clearNavMesh();

    if (navQuery) {
        dtFreeNavMeshQuery(navQuery);
        navQuery = nullptr;
    }
}

bool Navigation::setNavMesh(dtNavMesh* mesh, bool takeOwnership) {
    if (!mesh) {
        std::cerr << "Navigation::setNavMesh failed: mesh is null\n";
        return false;
    }

    clearNavMesh();

    if (!init(queryNodeLimit)) {
        return false;
    }

    navMesh = mesh;
    ownsNavMesh = takeOwnership;

    const dtStatus status = navQuery->init(navMesh, queryNodeLimit);
    if (dtStatusFailed(status)) {
        std::cerr << "Navigation::setNavMesh failed: dtNavMeshQuery::init failed with status " << status << "\n";
        clearNavMesh();
        return false;
    }

    return true;
}

bool Navigation::loadSingleTile(const dtNavMeshParams& params, unsigned char* tileData, int tileDataSize, int tileFlags) {
    if (!tileData || tileDataSize <= 0) {
        std::cerr << "Navigation::loadSingleTile failed: invalid tile data\n";
        return false;
    }

    clearNavMesh();

    dtNavMesh* mesh = dtAllocNavMesh();
    if (!mesh) {
        std::cerr << "Navigation::loadSingleTile failed: dtAllocNavMesh returned null\n";
        return false;
    }

    dtStatus status = mesh->init(&params);
    if (dtStatusFailed(status)) {
        std::cerr << "Navigation::loadSingleTile failed: dtNavMesh::init failed with status " << status << "\n";
        dtFreeNavMesh(mesh);
        return false;
    }

    dtTileRef tileRef = 0;
    status = mesh->addTile(tileData, tileDataSize, tileFlags, 0, &tileRef);
    if (dtStatusFailed(status)) {
        std::cerr << "Navigation::loadSingleTile failed: addTile failed with status " << status << "\n";
        dtFreeNavMesh(mesh);
        return false;
    }

    if (!setNavMesh(mesh, true)) {
        return false;
    }

    return true;
}

void Navigation::clearNavMesh() {
    if (ownsNavMesh && navMesh) {
        dtFreeNavMesh(navMesh);
    }

    navMesh = nullptr;
    ownsNavMesh = false;
}

bool Navigation::projectPointToNav(
    const glm::vec3& point,
    const glm::vec3& halfExtents,
    glm::vec3& outPoint,
    dtPolyRef* outRef,
    const dtQueryFilter* filter
) const {
    if (!navQuery || !navMesh) {
        return false;
    }

    const dtQueryFilter* f = filter ? filter : &defaultFilter;

    float p[3];
    float ext[3];
    toDetour(point, p);
    toDetour(halfExtents, ext);

    dtPolyRef ref = 0;
    float nearest[3] = {0.0f, 0.0f, 0.0f};
    const dtStatus status = navQuery->findNearestPoly(p, ext, f, &ref, nearest);
    if (dtStatusFailed(status) || ref == 0) {
        return false;
    }

    outPoint = fromDetour(nearest);
    if (outRef) {
        *outRef = ref;
    }

    return true;
}

bool Navigation::findPath(
    const glm::vec3& start,
    const glm::vec3& end,
    std::vector<glm::vec3>& outStraightPath,
    const glm::vec3& halfExtents,
    const dtQueryFilter* filter,
    int maxPolys,
    int maxStraightPoints
) const {
    outStraightPath.clear();

    if (!navQuery || !navMesh || maxPolys <= 0 || maxStraightPoints <= 0) {
        return false;
    }

    const dtQueryFilter* f = filter ? filter : &defaultFilter;

    float startPos[3];
    float endPos[3];
    float ext[3];
    toDetour(start, startPos);
    toDetour(end, endPos);
    toDetour(halfExtents, ext);

    dtPolyRef startRef = 0;
    dtPolyRef endRef = 0;
    float nearestStart[3];
    float nearestEnd[3];

    dtStatus status = navQuery->findNearestPoly(startPos, ext, f, &startRef, nearestStart);
    if (dtStatusFailed(status) || startRef == 0) {
        return false;
    }

    status = navQuery->findNearestPoly(endPos, ext, f, &endRef, nearestEnd);
    if (dtStatusFailed(status) || endRef == 0) {
        return false;
    }

    std::vector<dtPolyRef> polys(static_cast<size_t>(maxPolys));
    int polyCount = 0;
    status = navQuery->findPath(startRef, endRef, nearestStart, nearestEnd, f, polys.data(), &polyCount, maxPolys);
    if (dtStatusFailed(status) || polyCount <= 0) {
        return false;
    }

    std::vector<float> straightPath(static_cast<size_t>(maxStraightPoints) * 3u, 0.0f);
    std::vector<unsigned char> straightFlags(static_cast<size_t>(maxStraightPoints), 0);
    std::vector<dtPolyRef> straightPolys(static_cast<size_t>(maxStraightPoints), 0);
    int straightCount = 0;

    status = navQuery->findStraightPath(
        nearestStart,
        nearestEnd,
        polys.data(),
        polyCount,
        straightPath.data(),
        straightFlags.data(),
        straightPolys.data(),
        &straightCount,
        maxStraightPoints,
        DT_STRAIGHTPATH_AREA_CROSSINGS
    );

    if (dtStatusFailed(status) || straightCount <= 0) {
        return false;
    }

    outStraightPath.reserve(static_cast<size_t>(straightCount));
    for (int i = 0; i < straightCount; ++i) {
        const float* p = &straightPath[static_cast<size_t>(i) * 3u];
        outStraightPath.emplace_back(fromDetour(p));
    }

    return true;
}

bool Navigation::raycastPath(
    const glm::vec3& start,
    const glm::vec3& end,
    glm::vec3& outHitPos,
    bool& outHit,
    const glm::vec3& halfExtents,
    const dtQueryFilter* filter,
    int maxVisited
) const {
    outHit = false;

    if (!navQuery || !navMesh || maxVisited <= 0) {
        return false;
    }

    const dtQueryFilter* f = filter ? filter : &defaultFilter;

    float startPos[3];
    float endPos[3];
    float ext[3];
    toDetour(start, startPos);
    toDetour(end, endPos);
    toDetour(halfExtents, ext);

    dtPolyRef startRef = 0;
    float nearestStart[3] = {0.0f, 0.0f, 0.0f};
    dtStatus status = navQuery->findNearestPoly(startPos, ext, f, &startRef, nearestStart);
    if (dtStatusFailed(status) || startRef == 0) {
        return false;
    }

    std::vector<dtPolyRef> visited(static_cast<size_t>(maxVisited), 0);
    int visitedCount = 0;
    float t = 1.0f;
    float hitNormal[3] = {0.0f, 0.0f, 0.0f};

    status = navQuery->raycast(
        startRef,
        nearestStart,
        endPos,
        f,
        &t,
        hitNormal,
        visited.data(),
        &visitedCount,
        maxVisited
    );

    if (dtStatusFailed(status)) {
        return false;
    }

    float hitPos[3] = {
        nearestStart[0] + (endPos[0] - nearestStart[0]) * t,
        nearestStart[1] + (endPos[1] - nearestStart[1]) * t,
        nearestStart[2] + (endPos[2] - nearestStart[2]) * t
    };

    outHit = t < 1.0f;
    outHitPos = fromDetour(hitPos);
    return true;
}

void Navigation::toDetour(const glm::vec3& v, float out[3]) {
    out[0] = v.x;
    out[1] = v.y;
    out[2] = v.z;
}

glm::vec3 Navigation::fromDetour(const float in[3]) {
    return glm::vec3(in[0], in[1], in[2]);
}

} // namespace Core
} // namespace Lca
