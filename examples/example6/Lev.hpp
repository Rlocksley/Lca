#pragma once

#include "Level.hpp"
#include "AssetManager.hpp"
#include "Renderer.hpp"
#include "Helpers.hpp"
#include "Mesh.hpp"
#include "FlyingCamera.hpp"
#include "Light.hpp"
#include "Time.hpp"
#include "Vertex.hpp"

#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <sstream>

using namespace Lca;

// ──────────────────────────────────────────────────────────────
// Reusable low-poly mesh helpers (same as example5)
// ──────────────────────────────────────────────────────────────
namespace HardcodedMeshes6 {

    using V = Vertex::Mesh;

    struct MeshBuilder {
        std::vector<V>&         verts;
        std::vector<uint32_t>&  idx;

        void addQuad(glm::vec3 a, glm::vec3 b, glm::vec3 c, glm::vec3 d, glm::vec4 col) {
            glm::vec3 n = glm::normalize(glm::cross(d - a, b - a));
            uint32_t base = static_cast<uint32_t>(verts.size());
            verts.push_back({a, n, col, {0,0}});
            verts.push_back({b, n, col, {1,0}});
            verts.push_back({c, n, col, {1,1}});
            verts.push_back({d, n, col, {0,1}});
            idx.push_back(base);   idx.push_back(base+1); idx.push_back(base+2);
            idx.push_back(base);   idx.push_back(base+2); idx.push_back(base+3);
        }

        void addTri(glm::vec3 a, glm::vec3 b, glm::vec3 c, glm::vec4 col) {
            glm::vec3 n = glm::normalize(glm::cross(c - a, b - a));
            uint32_t base = static_cast<uint32_t>(verts.size());
            verts.push_back({a, n, col, {0,0}});
            verts.push_back({b, n, col, {1,0}});
            verts.push_back({c, n, col, {0.5f,1}});
            idx.push_back(base); idx.push_back(base+1); idx.push_back(base+2);
        }

        void addBox(glm::vec3 lo, glm::vec3 hi, glm::vec4 col) {
            addQuad({hi.x,lo.y,lo.z},{hi.x,lo.y,hi.z},{hi.x,hi.y,hi.z},{hi.x,hi.y,lo.z}, col);
            addQuad({lo.x,lo.y,hi.z},{lo.x,lo.y,lo.z},{lo.x,hi.y,lo.z},{lo.x,hi.y,hi.z}, col);
            addQuad({hi.x,lo.y,hi.z},{lo.x,lo.y,hi.z},{lo.x,hi.y,hi.z},{hi.x,hi.y,hi.z}, col);
            addQuad({lo.x,lo.y,lo.z},{hi.x,lo.y,lo.z},{hi.x,hi.y,lo.z},{lo.x,hi.y,lo.z}, col);
            addQuad({lo.x,hi.y,lo.z},{hi.x,hi.y,lo.z},{hi.x,hi.y,hi.z},{lo.x,hi.y,hi.z}, col);
            addQuad({lo.x,lo.y,hi.z},{hi.x,lo.y,hi.z},{hi.x,lo.y,lo.z},{lo.x,lo.y,lo.z}, col);
        }
    };

    inline void getHouseMesh(std::vector<V>& verts, std::vector<uint32_t>& idx, glm::vec4 wallCol) {
        verts.clear(); idx.clear();
        MeshBuilder mb{verts, idx};

        float wx = 2.5f, wy = 4.0f, wz = 2.5f;
        glm::vec4 roofCol(0.5f, 0.25f, 0.1f, 1.0f);
        glm::vec4 doorCol(0.3f, 0.15f, 0.05f, 1.0f);

        mb.addBox({-wx, 0, -wz}, {wx, wy, wz}, wallCol);

        float dw = 0.6f, dh = 2.0f, dz = wz + 0.01f;
        mb.addQuad({dw, 0, dz}, {-dw, 0, dz}, {-dw, dh, dz}, {dw, dh, dz}, doorCol);

        float rh = 2.0f, overhang = 0.3f;
        float rx = wx + overhang, rz = wz + overhang;
        mb.addQuad({-rx, wy, -rz}, {rx, wy, -rz}, {rx, wy + rh, 0}, {-rx, wy + rh, 0}, roofCol);
        mb.addQuad({rx, wy, rz}, {-rx, wy, rz}, {-rx, wy + rh, 0}, {rx, wy + rh, 0}, roofCol);
        mb.addTri({rx, wy, -rz}, {rx, wy, rz}, {rx, wy + rh, 0}, roofCol);
        mb.addTri({-rx, wy, rz}, {-rx, wy, -rz}, {-rx, wy + rh, 0}, roofCol);
    }

    inline void getCarMesh(std::vector<V>& verts, std::vector<uint32_t>& idx, glm::vec4 bodyCol) {
        verts.clear(); idx.clear();
        MeshBuilder mb{verts, idx};

        float bx = 2.0f, by = 1.2f, bz = 1.0f;
        mb.addBox({-bx, 0, -bz}, {bx, by, bz}, bodyCol);

        float cx0 = -0.5f, cx1 = 1.5f, cy2 = by + 1.0f, cz = 0.9f;
        mb.addBox({cx0, by, -cz}, {cx1, cy2, cz}, glm::vec4(0.6f, 0.8f, 1.0f, 1.0f));

        glm::vec4 wheelCol(0.15f, 0.15f, 0.15f, 1.0f);
        float wr = 0.35f;
        struct WP { float x, z; };
        WP wheels[] = {{1.3f, 1.1f}, {1.3f, -1.1f}, {-1.3f, 1.1f}, {-1.3f, -1.1f}};
        for (auto& w : wheels) {
            mb.addBox({w.x - wr, 0, w.z - 0.15f}, {w.x + wr, wr * 2.0f, w.z + 0.15f}, wheelCol);
        }
    }

    inline void getLanternMesh(std::vector<V>& verts, std::vector<uint32_t>& idx, glm::vec3 bulbColor) {
        verts.clear(); idx.clear();
        MeshBuilder mb{verts, idx};

        glm::vec4 poleCol(0.25f, 0.25f, 0.25f, 1.0f);
        float pw = 0.075f, ph = 5.0f;
        mb.addBox({-pw, 0, -pw}, {pw, ph, pw}, poleCol);

        float armLen = 1.0f, armTh = 0.05f;
        mb.addBox({0, ph - armTh, -armTh}, {armLen, ph + armTh, armTh}, poleCol);

        float bw = 0.2f, bh = 0.05f;
        mb.addBox({-bw, 0, -bw}, {bw, bh, bw}, poleCol);

        // Bulb
        glm::vec4 bulbCol(bulbColor, 1.0f);
        float lw = 0.2f, lh = 0.3f;
        mb.addBox({armLen - lw, ph - lh, -lw}, {armLen + lw, ph, lw}, bulbCol);
    }

    inline void getGroundMesh(std::vector<V>& verts, std::vector<uint32_t>& idx,
            float xMin, float xMax, float zMin, float zMax, glm::vec4 col) {
        verts.clear(); idx.clear();
        MeshBuilder mb{verts, idx};
        float y = -0.1f;
        mb.addQuad({xMin,y,zMin},{xMax,y,zMin},{xMax,y,zMax},{xMin,y,zMax}, col);
    }

    inline void getStreetMesh(std::vector<V>& verts, std::vector<uint32_t>& idx,
            float xMin, float xMax, float zMin, float zMax) {
        verts.clear(); idx.clear();
        MeshBuilder mb{verts, idx};
        glm::vec4 roadCol(0.22f, 0.22f, 0.24f, 1.0f);
        float y = 0.0f;
        float hw = 3.0f; // road half-width

        // Horizontal roads
        float zMid = (zMin + zMax) * 0.5f;
        mb.addQuad({xMin, y, zMin - hw}, {xMax, y, zMin - hw}, {xMax, y, zMin + hw}, {xMin, y, zMin + hw}, roadCol);
        mb.addQuad({xMin, y, zMax - hw}, {xMax, y, zMax - hw}, {xMax, y, zMax + hw}, {xMin, y, zMax + hw}, roadCol);
        mb.addQuad({xMin, y, zMid - hw}, {xMax, y, zMid - hw}, {xMax, y, zMid + hw}, {xMin, y, zMid + hw}, roadCol);

        // Vertical roads
        float xMid = (xMin + xMax) * 0.5f;
        mb.addQuad({xMin - hw, y, zMin}, {xMin + hw, y, zMin}, {xMin + hw, y, zMax}, {xMin - hw, y, zMax}, roadCol);
        mb.addQuad({xMax - hw, y, zMin}, {xMax + hw, y, zMin}, {xMax + hw, y, zMax}, {xMax - hw, y, zMax}, roadCol);
        mb.addQuad({xMid - hw, y, zMin}, {xMid + hw, y, zMin}, {xMid + hw, y, zMax}, {xMid - hw, y, zMax}, roadCol);
    }

} // namespace HardcodedMeshes6


// ──────────────────────────────────────────────────────────────
// Procedural texture generators (fire / water)
// ──────────────────────────────────────────────────────────────
namespace ProceduralTex {

    // Simple pseudo-random hash for per-pixel variation
    inline float hash(float x, float y) {
        float v = std::sin(x * 127.1f + y * 311.7f) * 43758.5453f;
        return v - std::floor(v);
    }

    // Value-noise helper (bilinear interp of hash grid)
    inline float noise(float x, float y) {
        float ix = std::floor(x), iy = std::floor(y);
        float fx = x - ix, fy = y - iy;
        float a = hash(ix, iy);
        float b = hash(ix + 1.0f, iy);
        float c = hash(ix, iy + 1.0f);
        float d = hash(ix + 1.0f, iy + 1.0f);
        float ux = fx * fx * (3.0f - 2.0f * fx);
        float uy = fy * fy * (3.0f - 2.0f * fy);
        return a + (b - a) * ux + (c - a) * uy + (a - b - c + d) * ux * uy;
    }

    // FBM (fractional Brownian motion)
    inline float fbm(float x, float y, int octaves = 4) {
        float val = 0.0f, amp = 0.5f;
        for (int i = 0; i < octaves; ++i) {
            val += amp * noise(x, y);
            x *= 2.0f;
            y *= 2.0f;
            amp *= 0.5f;
        }
        return val;
    }

    inline uint8_t clamp8(float v) {
        int i = static_cast<int>(v * 255.0f);
        return static_cast<uint8_t>(i < 0 ? 0 : (i > 255 ? 255 : i));
    }

    // Generate a 64x64 fire-like RGBA texture.
    // Each zone gets a unique seed so every texture is different.
    inline std::vector<uint32_t> generateFire(int w, int h, float seed) {
        std::vector<uint32_t> pixels(w * h);
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                float u = static_cast<float>(x) / w;
                float v = static_cast<float>(y) / h;

                // Upward gradient (hot at bottom, dark at top)
                float gradient = 1.0f - v;
                float n = fbm(u * 6.0f + seed, v * 6.0f + seed * 0.7f, 5);
                float flame = gradient * 0.7f + n * 0.5f;
                flame = std::fmax(0.0f, std::fmin(1.0f, flame));

                // Fire colour ramp: black → red → orange → yellow → white
                float r, g, b;
                if (flame < 0.25f) {
                    float t = flame / 0.25f;
                    r = t * 0.6f; g = 0.0f; b = 0.0f;
                } else if (flame < 0.55f) {
                    float t = (flame - 0.25f) / 0.3f;
                    r = 0.6f + t * 0.4f; g = t * 0.4f; b = 0.0f;
                } else if (flame < 0.8f) {
                    float t = (flame - 0.55f) / 0.25f;
                    r = 1.0f; g = 0.4f + t * 0.5f; b = t * 0.15f;
                } else {
                    float t = (flame - 0.8f) / 0.2f;
                    r = 1.0f; g = 0.9f + t * 0.1f; b = 0.15f + t * 0.55f;
                }

                uint32_t px = (255u << 24)
                    | (static_cast<uint32_t>(clamp8(b)) << 16)
                    | (static_cast<uint32_t>(clamp8(g)) << 8)
                    | (static_cast<uint32_t>(clamp8(r)));
                pixels[y * w + x] = px;
            }
        }
        return pixels;
    }

    // Generate a 64x64 water-like RGBA texture.
    inline std::vector<uint32_t> generateWater(int w, int h, float seed) {
        std::vector<uint32_t> pixels(w * h);
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                float u = static_cast<float>(x) / w;
                float v = static_cast<float>(y) / h;

                float n = fbm(u * 5.0f + seed, v * 5.0f + seed * 1.3f, 5);
                float wave = 0.5f + 0.5f * std::sin(u * 12.0f + n * 4.0f);
                float intensity = n * 0.4f + wave * 0.3f + 0.3f;
                intensity = std::fmax(0.0f, std::fmin(1.0f, intensity));

                float r = 0.05f + intensity * 0.15f;
                float g = 0.15f + intensity * 0.45f;
                float b = 0.35f + intensity * 0.60f;

                uint32_t px = (255u << 24)
                    | (static_cast<uint32_t>(clamp8(b)) << 16)
                    | (static_cast<uint32_t>(clamp8(g)) << 8)
                    | (static_cast<uint32_t>(clamp8(r)));
                pixels[y * w + x] = px;
            }
        }
        return pixels;
    }

} // namespace ProceduralTex


// ──────────────────────────────────────────────────────────────
// Helpers for the 10x10 city grid (shared between CityLevel
// and ZoneStreamingLevel)
// ──────────────────────────────────────────────────────────────

class ZoneStreamingLevel; // forward declaration for ZoneRegistry

// Singleton component that holds the zone map for the proximity system
struct ZoneRegistry {
    static constexpr int   kGridX    = 10;
    static constexpr int   kGridZ    = 10;
    static constexpr int   kZoneRows = 3;
    static constexpr int   kZoneCols = 3;
    static constexpr float kSpacing  = 14.0f;
    static constexpr float kGap      = 10.0f;

    // Stream-in / stream-out radii (world units from camera to zone center)
    float streamInRadius  = 200.0f;
    float streamOutRadius = 280.0f;

    std::shared_ptr<ZoneStreamingLevel> zones[kGridX * kGridZ];

    std::shared_ptr<ZoneStreamingLevel>& zone(int col, int row) {
        return zones[row * kGridX + col];
    }
};

namespace ZoneHelpers {

    // Generate a unique colour from grid position (HSV-based)
    inline glm::vec4 colorFromGrid(int col, int row, float s, float vMin, float vMax) {
        float hue = std::fmod((col * ZoneRegistry::kGridZ + row) * 137.508f, 360.0f);
        float v   = vMin + (vMax - vMin) * ((col + row) % 3) / 2.0f;
        float c = v * s;
        float x = c * (1.0f - std::fabs(std::fmod(hue / 60.0f, 2.0f) - 1.0f));
        float m = v - c;
        float r, g, b;
        if      (hue < 60)  { r=c; g=x; b=0; }
        else if (hue < 120) { r=x; g=c; b=0; }
        else if (hue < 180) { r=0; g=c; b=x; }
        else if (hue < 240) { r=0; g=x; b=c; }
        else if (hue < 300) { r=x; g=0; b=c; }
        else                { r=c; g=0; b=x; }
        return glm::vec4(r+m, g+m, b+m, 1.0f);
    }

    inline std::string zoneId(int col, int row) {
        std::ostringstream ss;
        ss << "zone_" << col << "_" << row;
        return ss.str();
    }

} // namespace ZoneHelpers


// ──────────────────────────────────────────────────────────────
// ZoneStreamingLevel — a streaming world chunk.
//
// Each zone covers a rectangular region of the world.  Multiple
// zones tile together like a continent.  They are streamed in / out
// in the background without ANY loading screen.
//
// Constructor params:
//   zoneId   — unique string, e.g. "zone_0_0"
//   originX  — world-space X origin
//   originZ  — world-space Z origin
//   rows/cols — grid size inside the zone
//   wallCol  — colour accent for this zone's houses
//   carCol   — colour accent for this zone's cars
//   lightCol — bulb colour for this zone's lanterns
// ──────────────────────────────────────────────────────────────

class ZoneStreamingLevel : public Lca::StreamingLevel {

    std::string zoneId;
    float originX, originZ;
    int   rows, cols;
    glm::vec4 wallCol, carCol;
    glm::vec3 lightCol;
    glm::vec4 groundCol;

    static constexpr float kSpacing = 14.0f;

    // Mesh names unique to this zone (so zones don't clobber each other)
    std::string houseMeshName, carMeshName, lanternMeshName,
                groundMeshName, streetMeshName;

    // Per-zone texture & material names
    std::string zoneTextureName;
    std::string zoneHouseMatName;
    bool        useFireTexture;   // true = fire, false = water

    // Entity names (for cleanup tracking — stored so cleanupScene can query them)
    std::vector<flecs::entity_t> ownedEntities;

public:
    ZoneStreamingLevel(const std::string& id,
                       float ox, float oz, int r, int c,
                       glm::vec4 wall, glm::vec4 car,
                       glm::vec3 light, glm::vec4 ground)
        : zoneId(id), originX(ox), originZ(oz),
          rows(r), cols(c),
          wallCol(wall), carCol(car), lightCol(light), groundCol(ground)
    {
        houseMeshName   = "house_"   + zoneId;
        carMeshName     = "car_"     + zoneId;
        lanternMeshName = "lantern_" + zoneId;
        groundMeshName  = "ground_"  + zoneId;
        streetMeshName  = "street_"  + zoneId;

        zoneTextureName = "tex_"     + zoneId;
        zoneHouseMatName= "hmat_"    + zoneId;

        // Alternate fire / water based on grid position hash
        // (even sum = fire, odd sum = water)
        int colIdx = 0, rowIdx = 0;
        if (sscanf(id.c_str(), "zone_%d_%d", &colIdx, &rowIdx) != 2) {
            colIdx = 0; rowIdx = 0;
        }
        useFireTexture = ((colIdx + rowIdx) % 2 == 0);
    }

    std::string getId() const override { return zoneId; }

    // World-space center of this zone (for proximity checks)
    glm::vec2 getCenter() const {
        return glm::vec2(originX + cols * kSpacing * 0.5f,
                         originZ + rows * kSpacing * 0.5f);
    }

    // ── Background thread: build zone meshes & texture ──
    void loadAssets() override {
        using V = Vertex::Mesh;
        std::vector<V> v;
        std::vector<uint32_t> i;

        HardcodedMeshes6::getHouseMesh(v, i, wallCol);
        Lca::Core::GetAssetManager().addMesh(houseMeshName, v, i);

        HardcodedMeshes6::getCarMesh(v, i, carCol);
        Lca::Core::GetAssetManager().addMesh(carMeshName, v, i);

        HardcodedMeshes6::getLanternMesh(v, i, lightCol);
        Lca::Core::GetAssetManager().addMesh(lanternMeshName, v, i);

        float xMin = originX;
        float xMax = originX + cols * kSpacing;
        float zMin = originZ;
        float zMax = originZ + rows * kSpacing;

        HardcodedMeshes6::getGroundMesh(v, i, xMin - 5.0f, xMax + 5.0f, zMin - 5.0f, zMax + 5.0f, groundCol);
        Lca::Core::GetAssetManager().addMesh(groundMeshName, v, i);

        HardcodedMeshes6::getStreetMesh(v, i, xMin, xMax, zMin, zMax);
        Lca::Core::GetAssetManager().addMesh(streetMeshName, v, i);

        // ── Per-zone procedural texture (fire or water) ──
        // Each zone gets a unique seed derived from its id hash so every
        // texture looks slightly different, simulating many unique assets.
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

        // Per-zone house material referencing the new texture
        Lca::Core::Material houseMat{};
        houseMat.textureId = static_cast<int32_t>(
            Lca::Core::GetAssetManager().getTextureId(zoneTextureName));
        houseMat.roughness = 0.9f;
        houseMat.metallic  = 0.0f;
        Lca::Core::GetAssetManager().addMaterial(zoneHouseMatName, houseMat);
    }

    // ── Main thread: create entities ──
    void setupScene(flecs::world& world) override {
        uint32_t pipelineId  = Lca::Core::GetRenderer().getMeshPipelineId("basic");
        uint32_t houseMeshId = Lca::Core::GetAssetManager().getMeshId(houseMeshName);
        uint32_t carMeshId   = Lca::Core::GetAssetManager().getMeshId(carMeshName);
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

        // Ground
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

        // Streets
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

        // Grid of buildings, cars, lanterns
        for (int row = 0; row < rows; ++row) {
            for (int col = 0; col < cols; ++col) {
                float bx = originX + col * kSpacing;
                float bz = originZ + row * kSpacing;

                // House
                {
                    auto parent = world.entity();
                    parent.set(Component::Transform{
                        glm::vec3(bx, 0.0f, bz),
                        0.0f, glm::vec3(0,1,0), glm::vec3(1.0f)});
                    parent.add<Component::TransformID>();
                    parent.add<Component::Static>();
                    track(parent);

                    auto mesh = world.entity();
                    mesh.add(flecs::ChildOf, parent);
                    mesh.set<Component::Mesh>({houseMeshId, houseMatId, pipelineId});
                }

                // Car
                {
                    float carAngle = ((row * cols + col) % 2 == 0) ? 0.0f : 3.14159f;
                    auto parent = world.entity();
                    parent.set(Component::Transform{
                        glm::vec3(bx + 5.0f, 0.0f, bz + 4.5f),
                        carAngle, glm::vec3(0,1,0), glm::vec3(1.0f)});
                    parent.add<Component::TransformID>();
                    parent.add<Component::Static>();
                    track(parent);

                    auto mesh = world.entity();
                    mesh.add(flecs::ChildOf, parent);
                    mesh.set<Component::Mesh>({carMeshId, carMatId, pipelineId});
                }

                // Lantern + point light
                {
                    auto parent = world.entity();
                    parent.set(Component::Transform{
                        glm::vec3(bx - 4.0f, 0.0f, bz + 5.5f),
                        0.0f, glm::vec3(0,1,0), glm::vec3(1.0f)});
                    parent.add<Component::TransformID>();
                    parent.add<Component::Static>();
                    track(parent);

                    auto mesh = world.entity();
                    mesh.add(flecs::ChildOf, parent);
                    mesh.set<Component::Mesh>({lanternId, lanternMatId, pipelineId});

                    auto light = world.entity();
                    light.set(Component::Transform{
                        glm::vec3(bx - 4.0f + 1.0f, 4.85f, bz + 5.5f),
                        0.0f, glm::vec3(0,1,0), glm::vec3(1.0f)});
                    light.add<Component::Static>();
                    light.set(Component::PointLight{
                        .color     = lightCol,
                        .intensity = 80.0f,
                        .radius    = 18.0f
                    });
                    track(light);
                }
            }
        }
    }

    // ── Main thread: destroy owned entities and remove zone assets ──
    void cleanupScene(flecs::world& world) override {
        for (auto id : ownedEntities) {
            auto e = world.entity(id);
            if (e.is_alive()) {
                e.destruct();
            }
        }
        ownedEntities.clear();

        // Remove zone-specific meshes so they can be re-added on next stream-in
        Lca::Core::GetAssetManager().removeMesh(houseMeshName);
        Lca::Core::GetAssetManager().removeMesh(carMeshName);
        Lca::Core::GetAssetManager().removeMesh(lanternMeshName);
        Lca::Core::GetAssetManager().removeMesh(groundMeshName);
        Lca::Core::GetAssetManager().removeMesh(streetMeshName);

        // Remove per-zone texture and material (GPU resources are destroyed —
        // Application::processStreaming will call updatePipelineDescriptorSets
        // afterwards so descriptor sets no longer reference stale handles)
        Lca::Core::GetAssetManager().removeMaterial(zoneHouseMatName);
        Lca::Core::GetAssetManager().removeTexture(zoneTextureName);
    }
};


// ──────────────────────────────────────────────────────────────
// CityLevel — loaded with loadLevel() at startup (loading screen).
//
// Owns the 10x10 streaming zone grid and the proximity ECS system.
// Different Level subclasses can have entirely different collections
// of streaming levels — each Level owns its own set.
// ──────────────────────────────────────────────────────────────

class CityLevel : public Lca::Level {
public:
    void loadAssets() override {
        using namespace Lca;

        // Shared materials (used by every streaming zone)
        Core::Material carMat{};
        carMat.textureId = -1;
        carMat.roughness = 0.3f;
        carMat.metallic  = 0.7f;
        Core::GetAssetManager().addMaterial("car_mat", carMat);

        Core::Material lanternMat{};
        lanternMat.textureId = -1;
        lanternMat.roughness = 0.3f;
        lanternMat.metallic  = 0.6f;
        Core::GetAssetManager().addMaterial("lantern_mat", lanternMat);

        Core::Material groundMat{};
        groundMat.textureId = -1;
        groundMat.roughness = 0.9f;
        groundMat.metallic  = 0.0f;
        Core::GetAssetManager().addMaterial("ground_mat", groundMat);

        Core::Material streetMat{};
        streetMat.textureId = -1;
        streetMat.roughness = 0.8f;
        streetMat.metallic  = 0.0f;
        Core::GetAssetManager().addMaterial("street_mat", streetMat);

        // Simulate a heavy initial load
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    void setupScene(flecs::world& world) override {
        using namespace Lca;

        // Persistent camera — survives streaming level changes
        world.entity("Player")
            .add<Component::Persistent>()
            .set<Component::FlyingCamera>({
                .position      = glm::vec3(0.0f, 40.0f, 100.0f),
                .angle         = Component::FlyingCamera::lookAt(
                    glm::vec3(0.0f, 40.0f, 100.0f), glm::vec3(0.0f, 0.0f, 0.0f)),
                .speed         = 40.0f,
                .rotationSpeed = 0.005f,
                .fov           = glm::radians(60.0f),
                .nearClip      = 0.1f,
                .farClip       = 3000.0f
            })
            .add<Component::Transform>();

        // ── Register the zone registry singleton and build 10x10 zones ──
        world.component<ZoneRegistry>();
        auto& reg = world.ensure<ZoneRegistry>();

        const float zoneW  = ZoneRegistry::kZoneCols * ZoneRegistry::kSpacing;
        const float zoneH  = ZoneRegistry::kZoneRows * ZoneRegistry::kSpacing;
        const float totalW = ZoneRegistry::kGridX * zoneW + (ZoneRegistry::kGridX - 1) * ZoneRegistry::kGap;
        const float totalH = ZoneRegistry::kGridZ * zoneH + (ZoneRegistry::kGridZ - 1) * ZoneRegistry::kGap;
        const float startX = -totalW * 0.5f;
        const float startZ = -totalH * 0.5f;

        for (int row = 0; row < ZoneRegistry::kGridZ; ++row) {
            for (int col = 0; col < ZoneRegistry::kGridX; ++col) {
                float ox = startX + col * (zoneW + ZoneRegistry::kGap);
                float oz = startZ + row * (zoneH + ZoneRegistry::kGap);

                glm::vec4 wallCol   = ZoneHelpers::colorFromGrid(col, row, 0.45f, 0.7f, 0.95f);
                glm::vec4 carCol    = ZoneHelpers::colorFromGrid(col, row, 0.8f,  0.4f, 0.8f);
                glm::vec3 lightCol  = glm::vec3(ZoneHelpers::colorFromGrid(col, row, 0.9f, 0.6f, 1.0f));
                glm::vec4 groundCol = ZoneHelpers::colorFromGrid(col, row, 0.3f,  0.25f, 0.45f);

                reg.zone(col, row) = std::make_shared<ZoneStreamingLevel>(
                    ZoneHelpers::zoneId(col, row), ox, oz,
                    ZoneRegistry::kZoneRows, ZoneRegistry::kZoneCols,
                    wallCol, carCol, lightCol, groundCol
                );
            }
        }

        // ── Proximity streaming system (runs every frame via ECS) ──
        // NOT tagged PersistentSystem — belongs to this level and is
        // cleaned up automatically when a different level is loaded.
        world.system<const Component::FlyingCamera>("ZoneProximityStreamer")
            .each([](flecs::entity e, const Component::FlyingCamera& cam) {
                auto w = e.world();
                if (!w.has<ZoneRegistry>()) return;
                const auto& reg = w.get<ZoneRegistry>();

                glm::vec2 camPos(cam.position.x, cam.position.z);
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
};
