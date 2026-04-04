#pragma once

#include "Level.hpp"
#include "AssetManager.hpp"
#include "Renderer.hpp"
#include "Helpers.hpp"
#include "Mesh.hpp"
#include "Light.hpp"
#include "Vertex.hpp"
#include "RigidBody.hpp"
#include "BoxCollider.hpp"
#include "Shape.hpp"
#include "ZoneRng.hpp"
#include "HardcodedMeshes6.hpp"
#include "ProceduralTex.hpp"
#include "ZoneGrid.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

using namespace Lca;

class ZoneStreamingLevel : public Lca::StreamingLevel {

    std::string zoneId;
    float originX, originZ;
    int   rows, cols;
    glm::vec4 wallCol, carCol;
    glm::vec3 lightCol;
    glm::vec4 groundCol;
    int   zoneType;           // 0=residential, 1=commercial, 2=downtown

    static constexpr float kSpacing = 14.0f;

    // ── Per-zone mesh / material variant names ──
    static constexpr int kBuildingVariants = 4;
    static constexpr int kCarVariants      = 3;

    std::string buildingMeshNames[kBuildingVariants];
    std::string carMeshNames[kCarVariants];

    std::string lanternMeshName, groundMeshName, streetMeshName;

    // Per-zone texture & material names
    std::string zoneTextureName;
    std::string zoneHouseMatName;
    bool        useFireTexture;

    // Seed for deterministic random layout
    uint32_t    layoutSeed;

    // Entity tracking for cleanup
    std::vector<flecs::entity_t> ownedEntities;

    // Physics half-extents for colliders (set during loadAssets)
    glm::vec3 buildingHalfExtents[kBuildingVariants];
    glm::vec3 carHalfExtents[kCarVariants];

public:
    ZoneStreamingLevel(const std::string& id,
                       float ox, float oz, int r, int c,
                       glm::vec4 wall, glm::vec4 car,
                       glm::vec3 light, glm::vec4 ground,
                       int type)
        : zoneId(id), originX(ox), originZ(oz),
          rows(r), cols(c),
          wallCol(wall), carCol(car), lightCol(light), groundCol(ground),
          zoneType(type)
    {
        for (int i = 0; i < kBuildingVariants; ++i)
            buildingMeshNames[i] = "bldg_" + zoneId + "_" + std::to_string(i);
        for (int i = 0; i < kCarVariants; ++i)
            carMeshNames[i] = "car_" + zoneId + "_" + std::to_string(i);

        lanternMeshName = "lantern_" + zoneId;
        groundMeshName  = "ground_"  + zoneId;
        streetMeshName  = "street_"  + zoneId;

        zoneTextureName  = "tex_"  + zoneId;
        zoneHouseMatName = "hmat_" + zoneId;

        layoutSeed = static_cast<uint32_t>(std::hash<std::string>{}(zoneId));

        // Alternate fire / water texture
        int colIdx = 0, rowIdx = 0;
        if (sscanf(id.c_str(), "zone_%d_%d", &colIdx, &rowIdx) != 2) {
            colIdx = 0; rowIdx = 0;
        }
        useFireTexture = ((colIdx + rowIdx) % 2 == 0);
    }

    std::string getId() const override { return zoneId; }

    glm::vec2 getCenter() const {
        return glm::vec2(originX + cols * kSpacing * 0.5f,
                         originZ + rows * kSpacing * 0.5f);
    }

    // ──────────────────────────────────────────────────────────
    // Background thread: build meshes, texture, material
    // ──────────────────────────────────────────────────────────
    void loadAssets() override {
        using V = Vertex::Mesh;
        std::vector<V> v;
        std::vector<uint32_t> idx;

        ZoneRng rng(layoutSeed);

        // ── Generate 4 building variants ──
        for (int i = 0; i < kBuildingVariants; ++i) {
            HardcodedMeshes6::BuildingParams bp;

            switch (zoneType) {
            case 0: // residential — small houses with peaked roofs
                bp.width      = rng.nextFloat(1.8f, 3.5f);
                bp.height     = rng.nextFloat(3.0f, 6.0f);
                bp.depth      = rng.nextFloat(1.8f, 3.5f);
                bp.roofHeight = rng.nextFloat(1.0f, 2.5f);
                bp.windowRows = rng.nextInt(1, 2);
                bp.windowCols = rng.nextInt(2, 3);
                bp.hasAntenna = false;
                break;

            case 1: // commercial — medium buildings, sometimes flat roof
                bp.width      = rng.nextFloat(2.5f, 5.0f);
                bp.height     = rng.nextFloat(5.0f, 12.0f);
                bp.depth      = rng.nextFloat(2.5f, 5.0f);
                bp.roofHeight = rng.nextBool(0.3f) ? rng.nextFloat(0.5f, 1.5f) : 0.0f;
                bp.windowRows = rng.nextInt(2, 5);
                bp.windowCols = rng.nextInt(3, 5);
                bp.hasAntenna = rng.nextBool(0.2f);
                break;

            case 2: // downtown — tall skyscrapers, flat roofs, antennas
                bp.width      = rng.nextFloat(2.0f, 4.5f);
                bp.height     = rng.nextFloat(10.0f, 30.0f);
                bp.depth      = rng.nextFloat(2.0f, 4.5f);
                bp.roofHeight = 0.0f;
                bp.windowRows = rng.nextInt(5, 12);
                bp.windowCols = rng.nextInt(3, 6);
                bp.hasAntenna = rng.nextBool(0.4f);
                break;
            }

            bp.wallCol = rng.varyColor(wallCol);
            bp.roofCol = glm::vec4(rng.nextFloat(0.3f, 0.6f),
                                   rng.nextFloat(0.15f, 0.4f),
                                   rng.nextFloat(0.05f, 0.2f), 1.0f);
            bp.doorCol   = glm::vec4(0.3f, 0.15f, 0.05f, 1.0f);
            bp.windowCol = glm::vec4(0.55f + rng.nextFloat(0.0f, 0.15f),
                                     0.75f + rng.nextFloat(0.0f, 0.15f),
                                     0.95f, 0.9f);

            HardcodedMeshes6::getBuildingMesh(v, idx, bp);
            Lca::Core::GetAssetManager().addMesh(buildingMeshNames[i], v, idx);
            buildingHalfExtents[i] = glm::vec3(bp.width, (bp.height + bp.roofHeight) * 0.5f, bp.depth);
        }

        // ── Generate 3 car variants (sedan / pickup / van) ──
        for (int i = 0; i < kCarVariants; ++i) {
            HardcodedMeshes6::CarParams cp;
            int carType = i;  // one of each type per zone

            switch (carType) {
            case 0: // sedan
                cp.bodyLength  = rng.nextFloat(1.6f, 2.2f);
                cp.bodyHeight  = rng.nextFloat(0.9f, 1.3f);
                cp.bodyWidth   = rng.nextFloat(0.8f, 1.1f);
                cp.cabinStart  = rng.nextFloat(-0.5f, -0.1f);
                cp.cabinEnd    = rng.nextFloat(1.0f, 1.5f);
                cp.cabinHeight = rng.nextFloat(0.7f, 1.0f);
                cp.hasBed      = false;
                break;

            case 1: // pickup truck
                cp.bodyLength  = rng.nextFloat(2.2f, 3.0f);
                cp.bodyHeight  = rng.nextFloat(1.1f, 1.5f);
                cp.bodyWidth   = rng.nextFloat(0.9f, 1.2f);
                cp.cabinStart  = rng.nextFloat(0.3f, 0.7f);
                cp.cabinEnd    = cp.bodyLength * rng.nextFloat(0.8f, 0.95f);
                cp.cabinHeight = rng.nextFloat(0.8f, 1.1f);
                cp.hasBed      = true;
                break;

            case 2: // van
                cp.bodyLength  = rng.nextFloat(2.0f, 2.8f);
                cp.bodyHeight  = rng.nextFloat(1.4f, 1.8f);
                cp.bodyWidth   = rng.nextFloat(0.9f, 1.2f);
                cp.cabinStart  = rng.nextFloat(-0.2f, 0.2f);
                cp.cabinEnd    = cp.bodyLength * rng.nextFloat(0.9f, 0.98f);
                cp.cabinHeight = rng.nextFloat(0.4f, 0.7f);
                cp.hasBed      = false;
                break;
            }

            cp.bodyCol = rng.varyColor(carCol, 0.2f);

            HardcodedMeshes6::getParametricCarMesh(v, idx, cp);
            Lca::Core::GetAssetManager().addMesh(carMeshNames[i], v, idx);
            carHalfExtents[i] = glm::vec3(cp.bodyLength, (cp.bodyHeight + cp.cabinHeight) * 0.5f, cp.bodyWidth);
        }

        // ── Lantern, ground, streets (unchanged) ──
        HardcodedMeshes6::getLanternMesh(v, idx, lightCol);
        Lca::Core::GetAssetManager().addMesh(lanternMeshName, v, idx);

        float xMin = originX;
        float xMax = originX + cols * kSpacing;
        float zMin = originZ;
        float zMax = originZ + rows * kSpacing;

        HardcodedMeshes6::getGroundMesh(v, idx, xMin - 5.0f, xMax + 5.0f,
                                                zMin - 5.0f, zMax + 5.0f, groundCol);
        Lca::Core::GetAssetManager().addMesh(groundMeshName, v, idx);

        HardcodedMeshes6::getStreetMesh(v, idx, xMin, xMax, zMin, zMax);
        Lca::Core::GetAssetManager().addMesh(streetMeshName, v, idx);

        // ── Per-zone procedural texture (fire or water) ──
        constexpr int kTexW = 64;
        constexpr int kTexH = 64;
        float seed = static_cast<float>(std::hash<std::string>{}(zoneId) % 10000) * 0.01f;

        std::vector<uint32_t> pixels;
        if (useFireTexture) {
            pixels = ProceduralTex::generateFire(kTexW, kTexH, seed);
        } else {
            pixels = ProceduralTex::generateWater(kTexW, kTexH, seed);
        }

        Lca::Core::Texture tex = Lca::Core::createTexture(kTexW, kTexH, pixels.data());
        Lca::Core::GetAssetManager().addTexture(zoneTextureName, tex);

        Lca::Core::Material houseMat{};
        houseMat.textureId = static_cast<int32_t>(
            Lca::Core::GetAssetManager().getTextureId(zoneTextureName));
        houseMat.roughness = 0.9f;
        houseMat.metallic  = 0.0f;
        Lca::Core::GetAssetManager().addMaterial(zoneHouseMatName, houseMat);
    }

    // ──────────────────────────────────────────────────────────
    // Main thread: create entities with randomised placement
    // ──────────────────────────────────────────────────────────
    void setupScene(flecs::world& world) override {
        uint32_t pipelineId   = Lca::Core::GetRenderer().getMeshPipelineId("basic");

        uint32_t bldgIds[kBuildingVariants];
        for (int i = 0; i < kBuildingVariants; ++i)
            bldgIds[i] = Lca::Core::GetAssetManager().getMeshId(buildingMeshNames[i]);

        uint32_t carIds[kCarVariants];
        for (int i = 0; i < kCarVariants; ++i)
            carIds[i] = Lca::Core::GetAssetManager().getMeshId(carMeshNames[i]);

        uint32_t lanternId   = Lca::Core::GetAssetManager().getMeshId(lanternMeshName);
        uint32_t groundId    = Lca::Core::GetAssetManager().getMeshId(groundMeshName);
        uint32_t streetId    = Lca::Core::GetAssetManager().getMeshId(streetMeshName);

        uint32_t houseMatId  = Lca::Core::GetAssetManager().getMaterialId(zoneHouseMatName);
        uint32_t carMatId    = Lca::Core::GetAssetManager().getMaterialId("car_mat");
        uint32_t lanternMatId= Lca::Core::GetAssetManager().getMaterialId("lantern_mat");
        uint32_t groundMatId = Lca::Core::GetAssetManager().getMaterialId("ground_mat");
        uint32_t streetMatId = Lca::Core::GetAssetManager().getMaterialId("street_mat");

        ownedEntities.clear();
        auto track = [this](flecs::entity e) { ownedEntities.push_back(e.id()); };

        // ── Ground ──
        {
            auto e = world.entity();
            e.set(Component::Transform{glm::vec3(0.0f), 0.0f, glm::vec3(0,1,0), glm::vec3(1.0f)});
            e.add<Component::TransformID>();
            e.add<Component::Static>();
            track(e);
            auto m = world.entity();
            m.add(flecs::ChildOf, e);
            m.set<Component::Mesh>({groundId, groundMatId, pipelineId});
        }

        // ── Streets ──
        {
            auto e = world.entity();
            e.set(Component::Transform{glm::vec3(0.0f), 0.0f, glm::vec3(0,1,0), glm::vec3(1.0f)});
            e.add<Component::TransformID>();
            e.add<Component::Static>();
            track(e);
            auto m = world.entity();
            m.add(flecs::ChildOf, e);
            m.set<Component::Mesh>({streetId, streetMatId, pipelineId});
        }

        // ── Per-cell RNG (different sequence from loadAssets) ──
        ZoneRng rng(layoutSeed + 54321u);

        for (int row = 0; row < rows; ++row) {
            for (int col = 0; col < cols; ++col) {
                float cellX = originX + col * kSpacing;
                float cellZ = originZ + row * kSpacing;

                // ────────── Buildings ──────────
                // Number of buildings per cell varies by zone type
                int numBuildings;
                switch (zoneType) {
                    case 0: numBuildings = rng.nextInt(1, 2); break;  // residential
                    case 1: numBuildings = rng.nextInt(1, 3); break;  // commercial
                    case 2: numBuildings = rng.nextInt(2, 3); break;  // downtown
                    default: numBuildings = 1; break;
                }

                for (int b = 0; b < numBuildings; ++b) {
                    int variant = rng.nextInt(0, kBuildingVariants - 1);
                    // Spread multiple buildings within the cell so they don't overlap
                    float offX = (numBuildings == 1) ? 0.0f : rng.nextFloat(-3.0f, 3.0f);
                    float offZ = (numBuildings == 1) ? 0.0f : rng.nextFloat(-3.0f, 3.0f);
                    float angle = rng.nextFloat(0.0f, 6.2832f);
                    float scale = rng.nextFloat(0.85f, 1.15f);

                    auto parent = world.entity();
                    parent.set(Component::Transform{
                        glm::vec3(cellX + offX, 0.0f, cellZ + offZ),
                        angle, glm::vec3(0,1,0), glm::vec3(scale)});
                    parent.add<Component::TransformID>();
                    parent.add<Component::Static>();
                    parent.set<Component::BoxCollider>({ buildingHalfExtents[variant] * scale });
                    parent.set<Component::StaticRigidBody>({ .objectLayer = Layers::STATIC_ENVIRONMENT, .friction = 0.8f, .restitution = 0.0f });
                    track(parent);

                    auto mesh = world.entity();
                    mesh.add(flecs::ChildOf, parent);
                    mesh.set<Component::Mesh>({bldgIds[variant], houseMatId, pipelineId});
                }

                // ────────── Cars ──────────
                int numCars;
                switch (zoneType) {
                    case 0: numCars = rng.nextInt(0, 1); break;
                    case 1: numCars = rng.nextInt(1, 2); break;
                    case 2: numCars = rng.nextInt(1, 3); break;
                    default: numCars = 1; break;
                }

                for (int c = 0; c < numCars; ++c) {
                    int variant = rng.nextInt(0, kCarVariants - 1);
                    // Place cars along the roadside
                    float carOffX = 5.0f + rng.nextFloat(-1.0f, 1.0f);
                    float carOffZ = 4.5f + c * rng.nextFloat(2.5f, 4.0f);
                    float carAngle = rng.nextBool() ? 0.0f : 3.14159f;

                    auto parent = world.entity();
                    parent.set(Component::Transform{
                        glm::vec3(cellX + carOffX, 0.0f, cellZ + carOffZ),
                        carAngle, glm::vec3(0,1,0), glm::vec3(1.0f)});
                    parent.add<Component::TransformID>();
                    parent.add<Component::Static>();
                    parent.set<Component::BoxCollider>({ carHalfExtents[variant] });
                    parent.set<Component::StaticRigidBody>({ .objectLayer = Layers::STATIC_ENVIRONMENT, .friction = 0.8f, .restitution = 0.0f });
                    track(parent);

                    auto mesh = world.entity();
                    mesh.add(flecs::ChildOf, parent);
                    mesh.set<Component::Mesh>({carIds[variant], carMatId, pipelineId});
                }

                // ────────── Lantern + point light ──────────
                {
                    auto parent = world.entity();
                    parent.set(Component::Transform{
                        glm::vec3(cellX - 4.0f, 0.0f, cellZ + 5.5f),
                        0.0f, glm::vec3(0,1,0), glm::vec3(1.0f)});
                    parent.add<Component::TransformID>();
                    parent.add<Component::Static>();
                    track(parent);

                    auto mesh = world.entity();
                    mesh.add(flecs::ChildOf, parent);
                    mesh.set<Component::Mesh>({lanternId, lanternMatId, pipelineId});

                    float lightIntensity = (zoneType == 2) ? 120.0f : 80.0f;
                    auto light = world.entity();
                    light.set(Component::Transform{
                        glm::vec3(cellX - 4.0f + 1.0f, 4.85f, cellZ + 5.5f),
                        0.0f, glm::vec3(0,1,0), glm::vec3(1.0f)});
                    light.add<Component::Static>();
                    light.set(Component::PointLight{
                        .color     = lightCol,
                        .intensity = lightIntensity,
                        .radius    = 18.0f
                    });
                    track(light);
                }
            }
        }
    }

    // ──────────────────────────────────────────────────────────
    // Main thread: destroy owned entities and remove zone assets
    // ──────────────────────────────────────────────────────────
    void cleanupScene(flecs::world& world) override {
        for (auto id : ownedEntities) {
            auto e = world.entity(id);
            if (e.is_alive()) e.destruct();
        }
        ownedEntities.clear();

        for (int i = 0; i < kBuildingVariants; ++i)
            Lca::Core::GetAssetManager().removeMesh(buildingMeshNames[i]);
        for (int i = 0; i < kCarVariants; ++i)
            Lca::Core::GetAssetManager().removeMesh(carMeshNames[i]);

        Lca::Core::GetAssetManager().removeMesh(lanternMeshName);
        Lca::Core::GetAssetManager().removeMesh(groundMeshName);
        Lca::Core::GetAssetManager().removeMesh(streetMeshName);

        Lca::Core::GetAssetManager().removeMaterial(zoneHouseMatName);
        Lca::Core::GetAssetManager().removeTexture(zoneTextureName);
    }
};


// ──────────────────────────────────────────────────────────────
// CityLevel — loaded with loadLevel() at startup (loading screen).
//
// Owns the 10x10 streaming zone grid and the proximity ECS system.
// Zone type is determined by distance from the grid centre:
//   inner ring  → downtown  (skyscrapers)
//   middle ring → commercial (medium buildings)
//   outer ring  → residential (small houses)
// ──────────────────────────────────────────────────────────────

