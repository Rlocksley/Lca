#pragma once

#include "Level.hpp"
#include "AssetManager.hpp"
#include "Renderer.hpp"
#include "Helpers.hpp"
#include "Mesh.hpp"
#include "MeshTransform.hpp"
#include "SkeletonMesh.hpp"
#include "FlyingCamera.hpp"
#include "CharacterCamera.hpp"
#include "AnimationStateMachine.hpp"
#include "Light.hpp"
#include "Time.hpp"
#include "Input.hpp"
#include "Vertex.hpp"
#include "CharacterCapsule.hpp"
#include "NavMesh.hpp"
#include "Navigation.hpp"
#include "RigidBody.hpp"
#include "BoxCollider.hpp"
#include "Velocity.hpp"
#include "Shape.hpp"
#include "ZoneRng.hpp"
#include "HardcodedMeshes6.hpp"
#include "ProceduralTex.hpp"
#include "Controllers.hpp"
#include "ZoneGrid.hpp"
#include "ZoneStreamingLevel.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include <recastnavigation/Recast.h>
#include <recastnavigation/DetourNavMeshBuilder.h>
#include <recastnavigation/DetourAlloc.h>

using namespace Lca;

namespace {

struct CityNavObstacle {
    glm::vec3 center;
    glm::vec3 halfExtents;
    float yaw;
};

inline void appendTri(std::vector<int>& tris, int a, int b, int c) {
    tris.push_back(a);
    tris.push_back(b);
    tris.push_back(c);
}

inline int appendVertex(std::vector<float>& verts, const glm::vec3& v) {
    verts.push_back(v.x);
    verts.push_back(v.y);
    verts.push_back(v.z);
    return static_cast<int>(verts.size() / 3u) - 1;
}

inline void appendQuad(
    std::vector<float>& verts,
    std::vector<int>& tris,
    std::vector<unsigned char>& triAreas,
    const glm::vec3& a,
    const glm::vec3& b,
    const glm::vec3& c,
    const glm::vec3& d,
    unsigned char area
) {
    const int ia = appendVertex(verts, a);
    const int ib = appendVertex(verts, b);
    const int ic = appendVertex(verts, c);
    const int id = appendVertex(verts, d);
    appendTri(tris, ia, ib, ic);
    triAreas.push_back(area);
    appendTri(tris, ia, ic, id);
    triAreas.push_back(area);
}

inline void appendOrientedBoxObstacle(
    std::vector<float>& verts,
    std::vector<int>& tris,
    std::vector<unsigned char>& triAreas,
    const CityNavObstacle& obs
) {
    const float c = std::cos(obs.yaw);
    const float s = std::sin(obs.yaw);

    auto rotate = [&](const glm::vec3& p) {
        return glm::vec3(
            p.x * c - p.z * s,
            p.y,
            p.x * s + p.z * c
        );
    };

    const glm::vec3 he = obs.halfExtents;

    std::array<glm::vec3, 8> local = {
        glm::vec3(-he.x, -he.y, -he.z),
        glm::vec3( he.x, -he.y, -he.z),
        glm::vec3( he.x, -he.y,  he.z),
        glm::vec3(-he.x, -he.y,  he.z),
        glm::vec3(-he.x,  he.y, -he.z),
        glm::vec3( he.x,  he.y, -he.z),
        glm::vec3( he.x,  he.y,  he.z),
        glm::vec3(-he.x,  he.y,  he.z)
    };

    std::array<int, 8> idx{};
    for (size_t i = 0; i < local.size(); ++i) {
        idx[i] = appendVertex(verts, obs.center + rotate(local[i]));
    }

    constexpr unsigned char blocked = RC_NULL_AREA;

    // Bottom
    appendTri(tris, idx[0], idx[2], idx[1]); triAreas.push_back(blocked);
    appendTri(tris, idx[0], idx[3], idx[2]); triAreas.push_back(blocked);

    // Top
    appendTri(tris, idx[4], idx[5], idx[6]); triAreas.push_back(blocked);
    appendTri(tris, idx[4], idx[6], idx[7]); triAreas.push_back(blocked);

    // Sides
    appendTri(tris, idx[0], idx[1], idx[5]); triAreas.push_back(blocked);
    appendTri(tris, idx[0], idx[5], idx[4]); triAreas.push_back(blocked);

    appendTri(tris, idx[1], idx[2], idx[6]); triAreas.push_back(blocked);
    appendTri(tris, idx[1], idx[6], idx[5]); triAreas.push_back(blocked);

    appendTri(tris, idx[2], idx[3], idx[7]); triAreas.push_back(blocked);
    appendTri(tris, idx[2], idx[7], idx[6]); triAreas.push_back(blocked);

    appendTri(tris, idx[3], idx[0], idx[4]); triAreas.push_back(blocked);
    appendTri(tris, idx[3], idx[4], idx[7]); triAreas.push_back(blocked);
}

inline int zoneTypeFromGrid(int col, int row) {
    const float centreCol = (ZoneRegistry::kGridX - 1) * 0.5f;
    const float centreRow = (ZoneRegistry::kGridZ - 1) * 0.5f;
    const float dx = col - centreCol;
    const float dz = row - centreRow;
    const float dist = std::sqrt(dx * dx + dz * dz);
    if (dist < 2.0f) return 2;
    if (dist < 4.0f) return 1;
    return 0;
}

// Replays the exact loadAssets() RNG sequence to recover per-variant
// building/car half-extents without needing ZoneStreamingLevel instances.
inline void computeZoneVariantExtents(uint32_t layoutSeed, int zoneType,
                                      glm::vec3 bldgHE[4], glm::vec3 carHE[3]) {
    ZoneRng rng(layoutSeed);

    // 4 building variants — mirror ZoneStreamingLevel::loadAssets()
    for (int i = 0; i < 4; ++i) {
        float width{}, height{}, depth{}, roofHeight{};
        switch (zoneType) {
        case 0:
            width      = rng.nextFloat(1.8f, 3.5f);
            height     = rng.nextFloat(3.0f, 6.0f);
            depth      = rng.nextFloat(1.8f, 3.5f);
            roofHeight = rng.nextFloat(1.0f, 2.5f);
            rng.nextInt(1, 2);   // windowRows
            rng.nextInt(2, 3);   // windowCols
            // hasAntenna = false (no RNG)
            break;
        case 1:
            width      = rng.nextFloat(2.5f, 5.0f);
            height     = rng.nextFloat(5.0f, 12.0f);
            depth      = rng.nextFloat(2.5f, 5.0f);
            roofHeight = rng.nextBool(0.3f) ? rng.nextFloat(0.5f, 1.5f) : 0.0f;
            rng.nextInt(2, 5);   // windowRows
            rng.nextInt(3, 5);   // windowCols
            rng.nextBool(0.2f);  // hasAntenna
            break;
        case 2:
        default:
            width      = rng.nextFloat(2.0f, 4.5f);
            height     = rng.nextFloat(10.0f, 30.0f);
            depth      = rng.nextFloat(2.0f, 4.5f);
            roofHeight = 0.0f;
            rng.nextInt(5, 12);  // windowRows
            rng.nextInt(3, 6);   // windowCols
            rng.nextBool(0.4f);  // hasAntenna
            break;
        }
        // varyColor(wallCol)  — 3 calls
        rng.nextFloat(0.0f, 1.0f); rng.nextFloat(0.0f, 1.0f); rng.nextFloat(0.0f, 1.0f);
        // roofCol — 3 calls
        rng.nextFloat(0.0f, 1.0f); rng.nextFloat(0.0f, 1.0f); rng.nextFloat(0.0f, 1.0f);
        // doorCol — hardcoded, 0 calls
        // windowCol — 2 calls
        rng.nextFloat(0.0f, 1.0f); rng.nextFloat(0.0f, 1.0f);

        bldgHE[i] = glm::vec3(width, (height + roofHeight) * 0.5f, depth);
    }

    // 3 car variants — mirror ZoneStreamingLevel::loadAssets()
    for (int i = 0; i < 3; ++i) {
        float bodyLength{}, bodyHeight{}, bodyWidth{}, cabinHeight{};
        switch (i) {
        case 0: // sedan
            bodyLength  = rng.nextFloat(1.6f, 2.2f);
            bodyHeight  = rng.nextFloat(0.9f, 1.3f);
            bodyWidth   = rng.nextFloat(0.8f, 1.1f);
            rng.nextFloat(0.0f, 1.0f); // cabinStart
            rng.nextFloat(0.0f, 1.0f); // cabinEnd
            cabinHeight = rng.nextFloat(0.7f, 1.0f);
            // hasBed = false
            break;
        case 1: // pickup
            bodyLength  = rng.nextFloat(2.2f, 3.0f);
            bodyHeight  = rng.nextFloat(1.1f, 1.5f);
            bodyWidth   = rng.nextFloat(0.9f, 1.2f);
            rng.nextFloat(0.0f, 1.0f); // cabinStart
            rng.nextFloat(0.0f, 1.0f); // cabinEnd
            cabinHeight = rng.nextFloat(0.8f, 1.1f);
            // hasBed = true
            break;
        case 2: // van
        default:
            bodyLength  = rng.nextFloat(2.0f, 2.8f);
            bodyHeight  = rng.nextFloat(1.4f, 1.8f);
            bodyWidth   = rng.nextFloat(0.9f, 1.2f);
            rng.nextFloat(0.0f, 1.0f); // cabinStart
            rng.nextFloat(0.0f, 1.0f); // cabinEnd
            cabinHeight = rng.nextFloat(0.4f, 0.7f);
            // hasBed = false
            break;
        }
        // varyColor(carCol, 0.2) — 3 calls
        rng.nextFloat(0.0f, 1.0f); rng.nextFloat(0.0f, 1.0f); rng.nextFloat(0.0f, 1.0f);

        carHE[i] = glm::vec3(bodyLength, (bodyHeight + cabinHeight) * 0.5f, bodyWidth);
    }
}

inline bool buildCityNavigationMesh() {
    std::vector<float> verts;
    std::vector<int> tris;
    std::vector<unsigned char> triAreas;
    verts.reserve(500000);
    tris.reserve(500000);
    triAreas.reserve(200000);

    constexpr int kBuildingVariants = 4;
    constexpr int kCarVariants      = 3;

    const float zoneW  = ZoneRegistry::kZoneCols * ZoneRegistry::kSpacing;
    const float zoneH  = ZoneRegistry::kZoneRows * ZoneRegistry::kSpacing;
    const float totalW = ZoneRegistry::kGridX * zoneW + (ZoneRegistry::kGridX - 1) * ZoneRegistry::kGap;
    const float totalH = ZoneRegistry::kGridZ * zoneH + (ZoneRegistry::kGridZ - 1) * ZoneRegistry::kGap;
    const float startX = -totalW * 0.5f;
    const float startZ = -totalH * 0.5f;

    // Ground/street surface is walkable.
    const float pad = 20.0f;
    const float x0 = startX - pad;
    const float z0 = startZ - pad;
    const float x1 = startX + totalW + pad;
    const float z1 = startZ + totalH + pad;
    appendQuad(
        verts, tris, triAreas,
        glm::vec3(x0, 0.0f, z0),
        glm::vec3(x1, 0.0f, z0),
        glm::vec3(x1, 0.0f, z1),
        glm::vec3(x0, 0.0f, z1),
        RC_WALKABLE_AREA
    );

    std::vector<CityNavObstacle> obstacles;
    obstacles.reserve(25000);

    for (int row = 0; row < ZoneRegistry::kGridZ; ++row) {
        for (int col = 0; col < ZoneRegistry::kGridX; ++col) {
            const float originX = startX + col * (zoneW + ZoneRegistry::kGap);
            const float originZ = startZ + row * (zoneH + ZoneRegistry::kGap);
            const int zoneType = zoneTypeFromGrid(col, row);

            const std::string id = ZoneHelpers::zoneId(col, row);
            const uint32_t layoutSeed = static_cast<uint32_t>(std::hash<std::string>{}(id));

            // Recover per-variant half-extents (mirrors loadAssets RNG)
            glm::vec3 bldgHE[kBuildingVariants];
            glm::vec3 carHE[kCarVariants];
            computeZoneVariantExtents(layoutSeed, zoneType, bldgHE, carHE);

            // Placement — mirrors ZoneStreamingLevel::setupScene() RNG exactly
            ZoneRng rng(layoutSeed + 54321u);

            for (int zr = 0; zr < ZoneRegistry::kZoneRows; ++zr) {
                for (int zc = 0; zc < ZoneRegistry::kZoneCols; ++zc) {
                    const float cellX = originX + zc * ZoneRegistry::kSpacing;
                    const float cellZ = originZ + zr * ZoneRegistry::kSpacing;

                    int numBuildings = 1;
                    switch (zoneType) {
                        case 0: numBuildings = rng.nextInt(1, 2); break;
                        case 1: numBuildings = rng.nextInt(1, 3); break;
                        case 2: numBuildings = rng.nextInt(2, 3); break;
                        default: break;
                    }

                    for (int b = 0; b < numBuildings; ++b) {
                        const int variant = rng.nextInt(0, kBuildingVariants - 1);
                        const float offX = (numBuildings == 1) ? 0.0f : rng.nextFloat(-3.0f, 3.0f);
                        const float offZ = (numBuildings == 1) ? 0.0f : rng.nextFloat(-3.0f, 3.0f);
                        const float yaw = rng.nextFloat(0.0f, 6.2832f);
                        const float scale = rng.nextFloat(0.85f, 1.15f);

                        obstacles.push_back({
                            glm::vec3(cellX + offX, 0.0f, cellZ + offZ),
                            bldgHE[variant] * scale,
                            yaw
                        });
                    }

                    int numCars = 0;
                    switch (zoneType) {
                        case 0: numCars = rng.nextInt(0, 1); break;
                        case 1: numCars = rng.nextInt(1, 2); break;
                        case 2: numCars = rng.nextInt(1, 3); break;
                        default: break;
                    }

                    for (int c = 0; c < numCars; ++c) {
                        const int variant = rng.nextInt(0, kCarVariants - 1);
                        const float carOffX = 5.0f + rng.nextFloat(-1.0f, 1.0f);
                        const float carOffZ = 4.5f + c * rng.nextFloat(2.5f, 4.0f);
                        const float carYaw = rng.nextBool() ? 0.0f : 3.14159f;
                        obstacles.push_back({
                            glm::vec3(cellX + carOffX, 0.0f, cellZ + carOffZ),
                            carHE[variant],
                            carYaw
                        });
                    }

                    // Lantern post as static obstacle volume.
                    obstacles.push_back({
                        glm::vec3(cellX - 4.0f, 0.0f, cellZ + 5.5f),
                        glm::vec3(0.35f, 2.5f, 0.35f),
                        0.0f
                    });
                }
            }
        }
    }

    for (const auto& obs : obstacles) {
        appendOrientedBoxObstacle(verts, tris, triAreas, obs);
    }

    if (tris.empty()) {
        std::cerr << "[Example6] Nav build failed: no triangles\n";
        return false;
    }

    rcContext ctx;
    rcConfig cfg{};
    cfg.cs = 0.6f;
    cfg.ch = 0.2f;
    cfg.walkableSlopeAngle = 45.0f;
    cfg.walkableHeight = static_cast<int>(std::ceil(2.0f / cfg.ch));
    cfg.walkableClimb = static_cast<int>(std::floor(0.9f / cfg.ch));
    cfg.walkableRadius = static_cast<int>(std::ceil(0.4f / cfg.cs));
    cfg.maxEdgeLen = static_cast<int>(12.0f / cfg.cs);
    cfg.maxSimplificationError = 1.3f;
    cfg.minRegionArea = rcSqr(8);
    cfg.mergeRegionArea = rcSqr(20);
    cfg.maxVertsPerPoly = 6;
    cfg.detailSampleDist = 6.0f * cfg.cs;
    cfg.detailSampleMaxError = 1.0f * cfg.ch;

    rcCalcBounds(verts.data(), static_cast<int>(verts.size() / 3u), cfg.bmin, cfg.bmax);
    rcCalcGridSize(cfg.bmin, cfg.bmax, cfg.cs, &cfg.width, &cfg.height);

    rcHeightfield* solid = rcAllocHeightfield();
    rcCompactHeightfield* chf = nullptr;
    rcContourSet* cset = nullptr;
    rcPolyMesh* pmesh = nullptr;
    rcPolyMeshDetail* dmesh = nullptr;

    bool ok = rcCreateHeightfield(&ctx, *solid, cfg.width, cfg.height, cfg.bmin, cfg.bmax, cfg.cs, cfg.ch);
    if (ok) ok = rcRasterizeTriangles(&ctx, verts.data(), static_cast<int>(verts.size() / 3u), tris.data(), triAreas.data(), static_cast<int>(triAreas.size()), *solid, cfg.walkableClimb);
    if (ok) rcFilterLowHangingWalkableObstacles(&ctx, cfg.walkableClimb, *solid);
    if (ok) rcFilterLedgeSpans(&ctx, cfg.walkableHeight, cfg.walkableClimb, *solid);
    if (ok) rcFilterWalkableLowHeightSpans(&ctx, cfg.walkableHeight, *solid);

    if (ok) {
        chf = rcAllocCompactHeightfield();
        ok = chf && rcBuildCompactHeightfield(&ctx, cfg.walkableHeight, cfg.walkableClimb, *solid, *chf);
    }
    if (ok) ok = rcErodeWalkableArea(&ctx, cfg.walkableRadius, *chf);
    if (ok) ok = rcBuildDistanceField(&ctx, *chf);
    if (ok) ok = rcBuildRegions(&ctx, *chf, 0, cfg.minRegionArea, cfg.mergeRegionArea);

    if (ok) {
        cset = rcAllocContourSet();
        ok = cset && rcBuildContours(&ctx, *chf, cfg.maxSimplificationError, cfg.maxEdgeLen, *cset);
    }
    if (ok) {
        pmesh = rcAllocPolyMesh();
        ok = pmesh && rcBuildPolyMesh(&ctx, *cset, cfg.maxVertsPerPoly, *pmesh);
    }
    if (ok) {
        dmesh = rcAllocPolyMeshDetail();
        ok = dmesh && rcBuildPolyMeshDetail(&ctx, *pmesh, *chf, cfg.detailSampleDist, cfg.detailSampleMaxError, *dmesh);
    }

    unsigned char* navData = nullptr;
    int navDataSize = 0;
    if (ok && pmesh->npolys > 0) {
        for (int i = 0; i < pmesh->npolys; ++i) {
            pmesh->flags[i] = (pmesh->areas[i] == RC_WALKABLE_AREA) ? 1 : 0;
        }

        dtNavMeshCreateParams params{};
        params.verts = pmesh->verts;
        params.vertCount = pmesh->nverts;
        params.polys = pmesh->polys;
        params.polyAreas = pmesh->areas;
        params.polyFlags = pmesh->flags;
        params.polyCount = pmesh->npolys;
        params.nvp = pmesh->nvp;
        params.detailMeshes = dmesh->meshes;
        params.detailVerts = dmesh->verts;
        params.detailVertsCount = dmesh->nverts;
        params.detailTris = dmesh->tris;
        params.detailTriCount = dmesh->ntris;
        params.walkableHeight = 2.0f;
        params.walkableRadius = 0.4f;
        params.walkableClimb = 0.9f;
        rcVcopy(params.bmin, pmesh->bmin);
        rcVcopy(params.bmax, pmesh->bmax);
        params.cs = cfg.cs;
        params.ch = cfg.ch;
        params.buildBvTree = true;

        ok = dtCreateNavMeshData(&params, &navData, &navDataSize);
    }

    if (!ok || !navData || navDataSize <= 0) {
        if (navData) {
            dtFree(navData);
        }
        if (dmesh) rcFreePolyMeshDetail(dmesh);
        if (pmesh) rcFreePolyMesh(pmesh);
        if (cset) rcFreeContourSet(cset);
        if (chf) rcFreeCompactHeightfield(chf);
        if (solid) rcFreeHeightField(solid);
        std::cerr << "[Example6] Nav build failed during Recast/Detour pipeline\n";
        return false;
    }

    dtNavMeshParams navParams{};
    rcVcopy(navParams.orig, pmesh->bmin);
    navParams.tileWidth = cfg.bmax[0] - cfg.bmin[0];
    navParams.tileHeight = cfg.bmax[2] - cfg.bmin[2];
    navParams.maxTiles = 1;
    navParams.maxPolys = 1 << 15;

    auto& nav = Core::GetNavigation();
    nav.init(65535);
    const bool loadOk = nav.loadSingleTile(navParams, navData, navDataSize, DT_TILE_FREE_DATA);

    if (dmesh) rcFreePolyMeshDetail(dmesh);
    if (pmesh) rcFreePolyMesh(pmesh);
    if (cset) rcFreeContourSet(cset);
    if (chf) rcFreeCompactHeightfield(chf);
    if (solid) rcFreeHeightField(solid);

    if (!loadOk) {
        std::cerr << "[Example6] Nav load failed\n";
        return false;
    }

    std::cout << "[Example6] Built navmesh from ground/houses/cars/lanterns. Triangles=" << (tris.size() / 3) << ", Obstacles=" << obstacles.size() << "\n";
    return true;
}

} // namespace

class CityLevel : public Lca::Level {
public:
    void loadAssets() override {
        using namespace Lca;

        auto& am = Core::GetAssetManager();

        // Shared materials (used by every streaming zone)
        Core::Material carMat{};
        carMat.textureId = -1;
        carMat.roughness = 0.3f;
        carMat.metallic  = 0.7f;
        am.addMaterial("car_mat", carMat);

        Core::Material lanternMat{};
        lanternMat.textureId = -1;
        lanternMat.roughness = 0.3f;
        lanternMat.metallic  = 0.6f;
        am.addMaterial("lantern_mat", lanternMat);

        Core::Material groundMat{};
        groundMat.textureId = -1;
        groundMat.roughness = 0.9f;
        groundMat.metallic  = 0.0f;
        am.addMaterial("ground_mat", groundMat);

        Core::Material streetMat{};
        streetMat.textureId = -1;
        streetMat.roughness = 0.8f;
        streetMat.metallic  = 0.0f;
        am.addMaterial("street_mat", streetMat);

        // ── Wizard skeleton model + animations ────────────────
        const std::string gltfPath = "C:/Users/robry/Desktop/3DModels/Wizard.gltf";
        model = am.loadSkeletonModel("Wizard", gltfPath);
        animNames = am.loadAnimations("Wizard", gltfPath);
        for (const auto& animName : animNames) {
            am.extractAnimationForSkeleton(animName, "Wizard");
        }

        // ── Skybox (inverted sphere + procedural sky texture) ──
        Shape::Sphere skySphere(1, 32, 32, glm::vec3(0.0f), glm::vec4(1.0f), true);
        am.addMesh("skyboxMesh", skySphere.getVertices(), skySphere.getIndices());

        constexpr int kSkyTexW = 1280;
        constexpr int kSkyTexH = 1280;
        auto skyPixels = ProceduralTex::generateSky(kSkyTexW, kSkyTexH, 400.0f);
        Core::Texture skyTex = Core::createTexture(kSkyTexW, kSkyTexH, skyPixels.data());
        am.addTexture("skyboxTex", skyTex);

        Core::Material skyMat{};
        skyMat.textureId = static_cast<int32_t>(am.getTextureId("skyboxTex"));
        skyMat.roughness = 1.0f;
        skyMat.metallic  = 0.0f;
        am.addMaterial("skyboxMat", skyMat);

        // Simulate a heavy initial load
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    void setupScene(flecs::world& world) override {
        using namespace Lca;

        auto& am  = Core::GetAssetManager();
        auto& ren = Core::GetRenderer();

        uint32_t pipelineId     = ren.getSkeletonMeshPipelineId("skeletonPBR");
        uint32_t meshPipelineId = ren.getMeshPipelineId("basic");
        constexpr float playerCapsuleHeight = 1.0f;
        constexpr float playerCapsuleRadius = 0.3f;

        // ── Skybox ────────────────────────────────────────────
        {
            auto e = world.entity("Skybox");
            e.add<Component::Persistent>();
            e.set(Component::Transform{
                glm::vec3(0.0f),
                glm::radians(-90.0f), glm::vec3(1,0,0),
                glm::vec3(400.0f)});
            e.add<Component::TransformID>();
            e.add<Component::Static>();

            auto m = world.entity("SkyboxMesh");
            m.add<Component::Persistent>();
            m.add(flecs::ChildOf, e);
            m.set<Component::Mesh>({
                am.getMeshId("skyboxMesh"),
                am.getMaterialId("skyboxMat"),
                meshPipelineId
            });
        }

        // ── Static rigid-body floor (very large, covers the entire city grid) ──
        {
            glm::vec3 floorHalfExtent(500.0f, 0.5f, 500.0f);
            auto floor = world.entity("Floor");
            floor.add<Component::Persistent>();
            floor.set(Component::Transform{glm::vec3(0.0f, -0.5f, 0.0f), 0.0f, glm::vec3(0,1,0), glm::vec3(floorHalfExtent)});
            floor.add<Component::TransformID>();
            floor.set<Component::BoxCollider>({ floorHalfExtent });
            floor.set<Component::StaticRigidBody>({ .objectLayer = Layers::STATIC_ENVIRONMENT, .friction = 0.8f, .restitution = 0.0f });
        }

        // ── Wizard player entity ──────────────────────────────
        auto player = world.entity("Player");
        player.add<Component::Persistent>();
        const float floorTopY = 0.0f;
        const float playerCenterY = floorTopY + (playerCapsuleHeight * 0.5f) + playerCapsuleRadius;
        const float meshLocalYOffset = -(playerCapsuleHeight * 0.5f + playerCapsuleRadius);
        player.set(Component::Transform{glm::vec3(0.0f, playerCenterY, 0.0f), 0.0f, glm::vec3(0,1,0), glm::vec3(1.0f)});
        player.set(Component::MeshTransform{Component::Transform{glm::vec3(0.0f, meshLocalYOffset, 0.0f), 0.0f, glm::vec3(0,1,0), glm::vec3(1.0f)}});
        player.add<Component::TransformID>();
        player.add<Component::SkeletonInstanceID>();

        for (size_t k = 0; k < model.size(); ++k) {
            const auto& meshInst = model[k];
            auto child = world.entity(("PlayerMesh_" + std::to_string(k)).c_str());
            child.add<Component::Persistent>();
            child.add(flecs::ChildOf, player);
            child.set<Component::SkeletonMesh>({
                .skeletonName = "Wizard",
                .meshID       = meshInst.meshId,
                .materialID   = meshInst.materialId,
                .pipelineID   = pipelineId,
                .nodeIndex    = meshInst.nodeIndex
            });
        }

        // ── Physics CharacterCapsule ──────────────────────────
        player.set<Component::CharacterCapsule>({
            .capsuleHeight = playerCapsuleHeight,
            .capsuleRadius = playerCapsuleRadius,
            .layer         = Layers::PLAYER_BODY,
            .speedDecrement = 4.0f
        });

        // ── Animation State Machine ───────────────────────────
        auto animKey = [](const std::string& rawName) -> std::string {
            return "Wizard:" + rawName;
        };

        std::string idleName, walkName, runName, spell2Name, deathName;
        for (const auto& n : animNames) {
            std::string lower = n;
            std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
            if (lower.find("idle")   != std::string::npos) idleName   = n;
            if (lower.find("walk")   != std::string::npos) walkName   = n;
            if (lower.find("run")    != std::string::npos) runName    = n;
            if (lower.find("spell2") != std::string::npos) spell2Name = n;
            if (lower.find("death")  != std::string::npos || lower.find("die") != std::string::npos) deathName = n;
        }

        Component::AnimationStateMachine asm_;
        asm_.skeletonName = "Wizard";

        if (!idleName.empty() && !walkName.empty() && !runName.empty()) {
            Component::BlendSpace1D blendSpace;
            blendSpace.pointCount = 3;
            blendSpace.animationNames[0] = animKey(idleName);
            blendSpace.animationNames[1] = animKey(walkName);
            blendSpace.animationNames[2] = animKey(runName);
            blendSpace.animationPoints[0] = 0.0f;
            blendSpace.animationPoints[1] = 0.5f;
            blendSpace.animationPoints[2] = 1.0f;
            blendSpace.blendParameter     = 0.0f;
            blendSpace.animationSpeed[0]  = 1.0f;
            blendSpace.animationSpeed[1]  = 1.0f;
            blendSpace.animationSpeed[2]  = 1.0f;
            asm_.addState("IdleWalkRun", blendSpace);
            asm_.setCurrentState("IdleWalkRun");
        } else if (!idleName.empty()) {
            Component::SingleAnimation idle;
            idle.animationName  = animKey(idleName);
            idle.animationSpeed = 1.0f;
            idle.looping        = true;
            asm_.addState("IdleWalkRun", idle);
            asm_.setCurrentState("IdleWalkRun");
        }

        if (!spell2Name.empty()) {
            Component::SingleAnimation spell;
            spell.animationName  = animKey(spell2Name);
            spell.animationSpeed = 1.0f;
            spell.looping        = false;
            asm_.addState("Spell2", spell);

            const auto& skelData = am.getSkeletonData("Wizard");
            std::vector<uint32_t> upperBodyMask;
            const std::vector<std::string> upperBoneNames = {
                "Torso", "Neck", "Head",
                "Shoulder.L", "UpperArm.L", "LowerArm.L", "Fist.L",
                "Shoulder.R", "UpperArm.R", "LowerArm.R", "Fist.R",
                "mixamorig:Spine1", "mixamorig:Spine2",
                "mixamorig:Neck", "mixamorig:Head",
                "mixamorig:LeftShoulder",  "mixamorig:LeftArm",  "mixamorig:LeftForeArm",  "mixamorig:LeftHand",
                "mixamorig:RightShoulder", "mixamorig:RightArm", "mixamorig:RightForeArm", "mixamorig:RightHand"
            };
            for (const auto& boneName : upperBoneNames) {
                auto it = skelData.nodeNameToIndex.find(boneName);
                if (it != skelData.nodeNameToIndex.end()) {
                    upperBodyMask.push_back(static_cast<uint32_t>(it->second));
                }
            }

            if (!upperBodyMask.empty()) {
                Component::ActionAnimationBlend attack;
                attack.actionState       = "Spell2";
                attack.baseState         = "IdleWalkRun";
                attack.nodeMask          = upperBodyMask;
                attack.blendInDuration   = 0.25f;
                attack.blendOutDuration  = 0.25f;
                asm_.addState("Attack", attack);
            }
        }

        if (!deathName.empty()) {
            Component::SingleAnimation death;
            death.animationName  = animKey(deathName);
            death.animationSpeed = 1.0f;
            death.looping        = false;
            asm_.addState("Death", death);
        }

        player.set(asm_);
        player.set<CharacterController>({});

        // ── Character control system ──────────────────────────
        world.system<CharacterController, Component::CharacterCapsule, Component::AnimationStateMachine>(
            "Character Controller Update")
            .multi_threaded()
            .kind(flecs::PreUpdate)
            .each([](CharacterController& ctrl, Component::CharacterCapsule& capsule,
                     Component::AnimationStateMachine& asm_) {
                ctrl.update(capsule, asm_);
            });

        // ── Camera following the player ───────────────────────
        world.entity("Camera")
            .add<Component::Persistent>()
            .set<Component::CharacterCamera>({
                .target        = player,
                .yaw           = glm::pi<float>(),
                .pitch         = 0.15f,
                .distance      = 8.0f,
                .rotationSpeed = 0.005f,
                .fov           = glm::radians(60.0f),
                .nearClip      = 0.1f,
                .farClip       = 3000.0f,
                .targetOffset  = glm::vec3(0.0f, 1.5f, 0.0f)
            })
            .add<Component::Transform>();

        // ── Sun light ─────────────────────────────────────────
        world.entity("Sun")
            .add<Component::Persistent>()
            .set<Component::DirectionalLight>({
                .color     = glm::vec3(1.0f, 0.98f, 0.9f),
                .intensity = 2.0f,
            });

        const bool navMeshReady = buildCityNavigationMesh();
        if (!navMeshReady) {
            std::cout << "[Example6] Warning: navmesh unavailable, NPC navigation disabled fallback behavior.\n";
        }

        // ── Spawn 500 NPC wizards that wander the city ────────
        {
            constexpr int kNpcCount = 500;
            constexpr float npcCapsuleHeight = 1.0f;
            constexpr float npcCapsuleRadius = 0.3f;
            const float npcCenterY = floorTopY + (npcCapsuleHeight * 0.5f) + npcCapsuleRadius;
            const float npcMeshYOff = -(npcCapsuleHeight * 0.5f + npcCapsuleRadius);

            // Build a template ASM identical to the player's
            // (reuses the same animation names already resolved above)
            auto buildNpcASM = [&]() -> Component::AnimationStateMachine {
                Component::AnimationStateMachine a;
                a.skeletonName = "Wizard";

                if (!idleName.empty() && !walkName.empty() && !runName.empty()) {
                    Component::BlendSpace1D bs;
                    bs.pointCount = 3;
                    bs.animationNames[0] = animKey(idleName);
                    bs.animationNames[1] = animKey(walkName);
                    bs.animationNames[2] = animKey(runName);
                    bs.animationPoints[0] = 0.0f;
                    bs.animationPoints[1] = 0.5f;
                    bs.animationPoints[2] = 1.0f;
                    bs.blendParameter     = 0.0f;
                    bs.animationSpeed[0]  = 1.0f;
                    bs.animationSpeed[1]  = 1.0f;
                    bs.animationSpeed[2]  = 1.0f;
                    a.addState("IdleWalkRun", bs);
                    a.setCurrentState("IdleWalkRun");
                } else if (!idleName.empty()) {
                    Component::SingleAnimation idle;
                    idle.animationName  = animKey(idleName);
                    idle.animationSpeed = 1.0f;
                    idle.looping        = true;
                    a.addState("IdleWalkRun", idle);
                    a.setCurrentState("IdleWalkRun");
                }

                if (!deathName.empty()) {
                    Component::SingleAnimation death;
                    death.animationName  = animKey(deathName);
                    death.animationSpeed = 1.0f;
                    death.looping        = false;
                    a.addState("Death", death);
                }
                return a;
            };

            ZoneRng npcRng(42u);
            for (int i = 0; i < kNpcCount; ++i) {
                // Spawn around the player's starting area (city center near origin).
                const float angle = npcRng.nextFloat(0.0f, 6.2832f);
                const float radius = npcRng.nextFloat(12.0f, 75.0f);
                float nx = std::cos(angle) * radius;
                float nz = std::sin(angle) * radius;
                glm::vec3 spawnPos(nx, npcCenterY, nz);
                if (navMeshReady) {
                    glm::vec3 projected;
                    if (Core::GetNavigation().projectPointToNav(spawnPos, glm::vec3(2.0f, 4.0f, 2.0f), projected)) {
                        spawnPos = projected;
                        spawnPos.y = npcCenterY;
                    }
                }

                std::string npcName = "NPC_" + std::to_string(i);
                auto npc = world.entity(npcName.c_str());
                npc.set(Component::Transform{
                    spawnPos,
                    0.0f, glm::vec3(0,1,0), glm::vec3(1.0f)});
                npc.set(Component::MeshTransform{Component::Transform{
                    glm::vec3(0.0f, npcMeshYOff, 0.0f),
                    0.0f, glm::vec3(0,1,0), glm::vec3(1.0f)}});
                npc.add<Component::TransformID>();
                npc.add<Component::SkeletonInstanceID>();

                // Skeleton mesh children (same sub-meshes as the player)
                for (size_t k = 0; k < model.size(); ++k) {
                    const auto& mi = model[k];
                    auto child = world.entity((npcName + "_Mesh_" + std::to_string(k)).c_str());
                    child.add(flecs::ChildOf, npc);
                    child.set<Component::SkeletonMesh>({
                        .skeletonName = "Wizard",
                        .meshID       = mi.meshId,
                        .materialID   = mi.materialId,
                        .pipelineID   = pipelineId,
                        .nodeIndex    = mi.nodeIndex
                    });
                }

                // Physics capsule
                npc.set<Component::CharacterCapsule>({
                    .capsuleHeight  = npcCapsuleHeight,
                    .capsuleRadius  = npcCapsuleRadius,
                    .layer          = Layers::ENEMY_BODY,
                    .speedDecrement = 4.0f
                });

                npc.set<Component::NavAgent>({
                    .projectionHalfExtents = glm::vec3(2.0f, 4.0f, 2.0f),
                    .repathInterval = 0.35f,
                    .cornerReachDistance = 1.0f,
                    .maxSpeed = 2.5f,
                    .repathTimer = 0.0f,
                    .requestRepath = true
                });
                npc.set<Component::NavTarget>({
                    .worldPos = spawnPos,
                    .active = false
                });
                npc.set<Component::NavPath>({});
                npc.set<Component::NavSteering>({});

                // Animation
                npc.set(buildNpcASM());

                // NPC AI controller — each gets unique RNG seed & varied pace
                {
                    NpcController ctrl;
                    ctrl.rng = static_cast<uint32_t>(i * 7919u + 12345u);
                    ctrl.preferredPace = npcRng.nextFloat(0.25f, 0.75f);
                    ctrl.idleTimer = npcRng.nextFloat(0.0f, 2.0f);
                    npc.set(ctrl);
                }
            }

            std::cout << "[Example6] Spawned " << kNpcCount << " NPC wizards.\n";
        }

        // ── NPC Controller system ─────────────────────────────
        world.system<NpcController, Component::NavAgent, Component::NavTarget,
                 const Component::NavSteering, Component::CharacterCapsule,
                 Component::AnimationStateMachine, const Component::Transform>(
            "NPC Nav Controller Update")
            .kind(flecs::PreUpdate)
            .each([](flecs::entity e, NpcController& ctrl,
                 Component::NavAgent& navAgent,
                 Component::NavTarget& navTarget,
                 const Component::NavSteering& navSteering,
                     Component::CharacterCapsule& capsule,
                     Component::AnimationStateMachine& asm_,
                     const Component::Transform& tf) {
            ctrl.update(tf, navAgent, navTarget, navSteering, capsule, asm_, Core::GetNavigation().isInitialized());
            });

        // ── Register the zone registry singleton and build 10x10 zones ──
        world.component<ZoneRegistry>();
        auto& reg = world.ensure<ZoneRegistry>();

        const float zoneW  = ZoneRegistry::kZoneCols * ZoneRegistry::kSpacing;
        const float zoneH  = ZoneRegistry::kZoneRows * ZoneRegistry::kSpacing;
        const float totalW = ZoneRegistry::kGridX * zoneW + (ZoneRegistry::kGridX - 1) * ZoneRegistry::kGap;
        const float totalH = ZoneRegistry::kGridZ * zoneH + (ZoneRegistry::kGridZ - 1) * ZoneRegistry::kGap;
        const float startX = -totalW * 0.5f;
        const float startZ = -totalH * 0.5f;

        const float centreCol = (ZoneRegistry::kGridX - 1) * 0.5f;   // 4.5
        const float centreRow = (ZoneRegistry::kGridZ - 1) * 0.5f;

        for (int row = 0; row < ZoneRegistry::kGridZ; ++row) {
            for (int col = 0; col < ZoneRegistry::kGridX; ++col) {
                float ox = startX + col * (zoneW + ZoneRegistry::kGap);
                float oz = startZ + row * (zoneH + ZoneRegistry::kGap);

                glm::vec4 wallCol   = ZoneHelpers::colorFromGrid(col, row, 0.45f, 0.7f, 0.95f);
                glm::vec4 carCol    = ZoneHelpers::colorFromGrid(col, row, 0.8f,  0.4f, 0.8f);
                glm::vec3 lightCol  = glm::vec3(ZoneHelpers::colorFromGrid(col, row, 0.9f, 0.6f, 1.0f));
                glm::vec4 groundCol = ZoneHelpers::colorFromGrid(col, row, 0.3f,  0.25f, 0.45f);

                // Zone type based on distance from grid centre
                float dx = col - centreCol;
                float dz = row - centreRow;
                float dist = std::sqrt(dx * dx + dz * dz);
                int zoneType;
                if (dist < 2.0f)      zoneType = 2;  // downtown
                else if (dist < 4.0f) zoneType = 1;  // commercial
                else                  zoneType = 0;  // residential

                reg.zone(col, row) = std::make_shared<ZoneStreamingLevel>(
                    ZoneHelpers::zoneId(col, row), ox, oz,
                    ZoneRegistry::kZoneRows, ZoneRegistry::kZoneCols,
                    wallCol, carCol, lightCol, groundCol,
                    zoneType
                );
            }
        }

        // ── Proximity streaming system (runs every frame via ECS) ──
        // Tracks the Player entity's Transform position for zone streaming.
        // Filter on CharacterController to match only the player, not NPCs.
        world.system<const Component::Transform, const Component::CharacterCapsule>("ZoneProximityStreamer")
            .with<CharacterController>()
            .each([](flecs::entity e, const Component::Transform& tf, const Component::CharacterCapsule&) {
                auto w = e.world();
                if (!w.has<ZoneRegistry>()) return;
                const auto& reg = w.get<ZoneRegistry>();

                glm::vec2 camPos(tf.position.x, tf.position.z);
                float inR2  = reg.streamInRadius  * reg.streamInRadius;
                float outR2 = reg.streamOutRadius * reg.streamOutRadius;

                Lca::StreamInRequest  inReq;
                Lca::StreamOutRequest outReq;

                for (int i = 0; i < ZoneRegistry::kGridX * ZoneRegistry::kGridZ; ++i) {
                    auto& zone = reg.zones[i];
                    if (!zone) continue;

                    glm::vec2 center = zone->getCenter();
                    glm::vec2 diff   = camPos - center;
                    float dist2 = glm::dot(diff, diff);

                    // Application deduplicates streamIn/streamOut calls automatically.
                    if (dist2 <= inR2) {
                        inReq.levels.push_back(zone);
                    } else if (dist2 > outR2) {
                        outReq.levelIds.push_back(zone->getId());
                    }
                }

                if (!inReq.levels.empty()) {
                    if (w.has<Lca::StreamInRequest>()) {
                        auto& existing = w.get_mut<Lca::StreamInRequest>();
                        for (auto& l : inReq.levels) existing.levels.push_back(l);
                    } else {
                        w.set<Lca::StreamInRequest>(std::move(inReq));
                    }
                }
                if (!outReq.levelIds.empty()) {
                    if (w.has<Lca::StreamOutRequest>()) {
                        auto& existing = w.get_mut<Lca::StreamOutRequest>();
                        for (auto& id : outReq.levelIds) existing.levelIds.push_back(id);
                    } else {
                        w.set<Lca::StreamOutRequest>(std::move(outReq));
                    }
                }
            });
    }

    void cleanupScene(flecs::world& world) override {
        // Remove the ZoneRegistry singleton so zone shared_ptrs are released
        world.remove<ZoneRegistry>();

        // Delete all non-persistent entities (proximity system, etc.)
        deleteNonPersistentEntities(world);
    }

private:
    Core::Model model;
    std::vector<std::string> animNames;
};
