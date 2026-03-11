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

using namespace Lca;

// ──────────────────────────────────────────────────────────────
// Hard-coded low-poly meshes
// ──────────────────────────────────────────────────────────────
namespace HardcodedMeshes {

    using V = Vertex::Mesh;

    // Helper lambda-factories used by each mesh builder
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
            // +X
            addQuad({hi.x,lo.y,lo.z},{hi.x,lo.y,hi.z},{hi.x,hi.y,hi.z},{hi.x,hi.y,lo.z}, col);
            // -X
            addQuad({lo.x,lo.y,hi.z},{lo.x,lo.y,lo.z},{lo.x,hi.y,lo.z},{lo.x,hi.y,hi.z}, col);
            // +Z
            addQuad({hi.x,lo.y,hi.z},{lo.x,lo.y,hi.z},{lo.x,hi.y,hi.z},{hi.x,hi.y,hi.z}, col);
            // -Z
            addQuad({lo.x,lo.y,lo.z},{hi.x,lo.y,lo.z},{hi.x,hi.y,lo.z},{lo.x,hi.y,lo.z}, col);
            // +Y
            addQuad({lo.x,hi.y,lo.z},{hi.x,hi.y,lo.z},{hi.x,hi.y,hi.z},{lo.x,hi.y,hi.z}, col);
            // -Y
            addQuad({lo.x,lo.y,hi.z},{hi.x,lo.y,hi.z},{hi.x,lo.y,lo.z},{lo.x,lo.y,lo.z}, col);
        }
    };

    // ──────────────────────────────────────────────────
    // CAR – simplified box-car shape
    //   Body:  elongated box (4 x 1.2 x 2)
    //   Cabin: smaller box on top (2 x 1 x 1.8)
    //   4 wheels
    //   Color: red body, light-blue cabin, dark wheels
    // ──────────────────────────────────────────────────
    inline void getCarMesh(std::vector<V>& verts, std::vector<uint32_t>& idx) {
        verts.clear(); idx.clear();
        MeshBuilder mb{verts, idx};

        float bx = 2.0f, by = 1.2f, bz = 1.0f;
        glm::vec4 bodyCol(0.8f, 0.1f, 0.1f, 1.0f);

        // Body – 6 faces
        mb.addBox({-bx, 0, -bz}, {bx, by, bz}, bodyCol);

        // Cabin on top
        float cx0 = -0.5f, cx1 = 1.5f, cy2 = by + 1.0f, cz = 0.9f;
        mb.addBox({cx0, by, -cz}, {cx1, cy2, cz}, glm::vec4(0.6f, 0.8f, 1.0f, 1.0f));

        // 4 wheels
        glm::vec4 wheelCol(0.15f, 0.15f, 0.15f, 1.0f);
        float wr = 0.35f;
        struct WP { float x, z; };
        WP wheels[] = {{1.3f, 1.1f}, {1.3f, -1.1f}, {-1.3f, 1.1f}, {-1.3f, -1.1f}};
        for (auto& w : wheels) {
            mb.addBox({w.x - wr, 0, w.z - 0.15f}, {w.x + wr, wr * 2.0f, w.z + 0.15f}, wheelCol);
        }
    }

    // ──────────────────────────────────────────────────
    // HOUSE – box walls + triangular prism roof + door
    //   Walls: 5 x 4 x 5
    //   Roof:  triangular, rises 2 units above walls
    //   Color: beige walls, brown roof, dark door
    // ──────────────────────────────────────────────────
    inline void getHouseMesh(std::vector<V>& verts, std::vector<uint32_t>& idx) {
        verts.clear(); idx.clear();
        MeshBuilder mb{verts, idx};

        float wx = 2.5f, wy = 4.0f, wz = 2.5f;
        glm::vec4 wallCol(0.9f, 0.85f, 0.7f, 1.0f);
        glm::vec4 roofCol(0.5f, 0.25f, 0.1f, 1.0f);
        glm::vec4 doorCol(0.3f, 0.15f, 0.05f, 1.0f);

        // Walls
        mb.addBox({-wx, 0, -wz}, {wx, wy, wz}, wallCol);

        // Door on front face (+Z)
        float dw = 0.6f, dh = 2.0f, dz = wz + 0.01f;
        mb.addQuad({dw, 0, dz}, {-dw, 0, dz}, {-dw, dh, dz}, {dw, dh, dz}, doorCol);

        // Roof – triangular prism along X
        float rh = 2.0f, overhang = 0.3f;
        float rx = wx + overhang, rz = wz + overhang;

        // Two sloped faces
        mb.addQuad({-rx, wy, -rz}, {rx, wy, -rz}, {rx, wy + rh, 0}, {-rx, wy + rh, 0}, roofCol);
        mb.addQuad({rx, wy, rz}, {-rx, wy, rz}, {-rx, wy + rh, 0}, {rx, wy + rh, 0}, roofCol);

        // Two triangular gable ends
        mb.addTri({rx, wy, -rz}, {rx, wy, rz}, {rx, wy + rh, 0}, roofCol);
        mb.addTri({-rx, wy, rz}, {-rx, wy, -rz}, {-rx, wy + rh, 0}, roofCol);
    }

    // ──────────────────────────────────────────────────
    // STREET LANTERN – tall pole + horizontal arm + lamp
    //   Pole: thin box (0.15 x 5 x 0.15)
    //   Arm:  horizontal box at top
    //   Lamp: small box at end of arm, warm yellow
    //   Base: small plate
    // ──────────────────────────────────────────────────
    inline void getLanternPoleMesh(std::vector<V>& verts, std::vector<uint32_t>& idx) {
        verts.clear(); idx.clear();
        MeshBuilder mb{verts, idx};

        glm::vec4 poleCol(0.25f, 0.25f, 0.25f, 1.0f);

        // Pole
        float pw = 0.075f, ph = 5.0f;
        mb.addBox({-pw, 0, -pw}, {pw, ph, pw}, poleCol);

        // Arm at top
        float armLen = 1.0f, armTh = 0.05f;
        mb.addBox({0, ph - armTh, -armTh}, {armLen, ph + armTh, armTh}, poleCol);

        // Base plate
        float bw = 0.2f, bh = 0.05f;
        mb.addBox({-bw, 0, -bw}, {bw, bh, bw}, poleCol);
    }

    inline void getLanternBulbMesh(std::vector<V>& verts, std::vector<uint32_t>& idx, glm::vec3 color) {
        verts.clear(); idx.clear();
        MeshBuilder mb{verts, idx};

        glm::vec4 bulbCol(color, 1.0f);

        float ph = 5.0f;
        float armLen = 1.0f;
        float lw = 0.2f, lh = 0.3f;
        mb.addBox({armLen - lw, ph - lh, -lw}, {armLen + lw, ph, lw}, bulbCol);
    }

    // ──────────────────────────────────────────────────
    // GROUND – flat grass plane at y = -0.1
    // ──────────────────────────────────────────────────
    inline void getGroundMesh(std::vector<V>& verts, std::vector<uint32_t>& idx,
            float xMin, float xMax, float zMin, float zMax) {
        verts.clear(); idx.clear();
        MeshBuilder mb{verts, idx};
        glm::vec4 grassCol(0.28f, 0.42f, 0.18f, 1.0f);
        float y = -0.1f;
        mb.addQuad({xMin,y,zMin},{xMax,y,zMin},{xMax,y,zMax},{xMin,y,zMax}, grassCol);
    }

    // ──────────────────────────────────────────────────
    // STREET NETWORK – road quads running between blocks
    //   11 horizontal strips + 11 vertical strips
    //   streetHW: half-width of each road (3 units => 6-wide road)
    // ──────────────────────────────────────────────────
    inline void getStreetNetworkMesh(std::vector<V>& verts, std::vector<uint32_t>& idx,
            int gridCols, int gridRows, float spacingX, float spacingZ, float streetHW = 3.0f) {
        verts.clear(); idx.clear();
        MeshBuilder mb{verts, idx};
        glm::vec4 roadCol(0.22f, 0.22f, 0.24f, 1.0f);
        float y = 0.0f;

        float xMin = (0        - gridCols / 2.f) * spacingX - spacingX * 0.5f;
        float xMax = (gridCols - gridCols / 2.f) * spacingX - spacingX * 0.5f;
        float zMin = (0        - gridRows / 2.f) * spacingZ - spacingZ * 0.5f;
        float zMax = (gridRows - gridRows / 2.f) * spacingZ - spacingZ * 0.5f;

        // Horizontal roads – run along X, one per row boundary
        for (int r = 0; r <= gridRows; ++r) {
            float zc = (r - gridRows / 2.f) * spacingZ - spacingZ * 0.5f;
            mb.addQuad({xMin, y, zc - streetHW}, {xMax, y, zc - streetHW},
                       {xMax, y, zc + streetHW}, {xMin, y, zc + streetHW}, roadCol);
        }

        // Vertical roads – run along Z, one per column boundary
        for (int c = 0; c <= gridCols; ++c) {
            float xc = (c - gridCols / 2.f) * spacingX - spacingX * 0.5f;
            mb.addQuad({xc - streetHW, y, zMin}, {xc + streetHW, y, zMin},
                       {xc + streetHW, y, zMax}, {xc - streetHW, y, zMax}, roadCol);
        }
    }

} // namespace HardcodedMeshes


// ──────────────────────────────────────────────────────────────
// Game Level – 100 houses, 100 cars, 100 street lanterns
// ──────────────────────────────────────────────────────────────

class GameLevel : public Lca::Level {

    static constexpr float kSpacingX = 14.0f;
    static constexpr float kSpacingZ = 14.0f;
    static constexpr int   kGridCols = 10;
    static constexpr int   kGridRows = 10;

public:
    void loadAssets() override {

        // Register the 3 hardcoded meshes via addMesh
        {
            std::vector<Vertex::Mesh> v; std::vector<uint32_t> i;
            HardcodedMeshes::getCarMesh(v, i);
            Core::GetAssetManager().addMesh("car", v, i);
        }
        {
            std::vector<Vertex::Mesh> v; std::vector<uint32_t> i;
            HardcodedMeshes::getHouseMesh(v, i);
            Core::GetAssetManager().addMesh("house", v, i);
        }
        {
            std::vector<Vertex::Mesh> v; std::vector<uint32_t> i;
            HardcodedMeshes::getLanternPoleMesh(v, i);
            Core::GetAssetManager().addMesh("lantern_pole", v, i);
        }
        {
            std::vector<Vertex::Mesh> v; std::vector<uint32_t> i;
            HardcodedMeshes::getLanternBulbMesh(v, i, glm::vec3(1.0f, 0.0f, 0.0f));
            Core::GetAssetManager().addMesh("lantern_bulb_r", v, i);
        }
        {
            std::vector<Vertex::Mesh> v; std::vector<uint32_t> i;
            HardcodedMeshes::getLanternBulbMesh(v, i, glm::vec3(0.0f, 1.0f, 0.0f));
            Core::GetAssetManager().addMesh("lantern_bulb_g", v, i);
        }
        {
            std::vector<Vertex::Mesh> v; std::vector<uint32_t> i;
            HardcodedMeshes::getLanternBulbMesh(v, i, glm::vec3(0.0f, 0.0f, 1.0f));
            Core::GetAssetManager().addMesh("lantern_bulb_b", v, i);
        }

        // Materials
        Core::Material carMat{};
        carMat.textureId = -1;
        carMat.roughness = 0.3f;
        carMat.metallic  = 0.7f;
        Core::GetAssetManager().addMaterial("car_material", carMat);

        Core::Material houseMat{};
        houseMat.textureId = -1;
        houseMat.roughness = 0.9f;
        houseMat.metallic  = 0.0f;
        Core::GetAssetManager().addMaterial("house_material", houseMat);

        Core::Material lanternPoleMat{};
        lanternPoleMat.textureId = -1;
        lanternPoleMat.roughness = 0.4f;
        lanternPoleMat.metallic  = 0.8f;
        Core::GetAssetManager().addMaterial("lantern_pole_material", lanternPoleMat);

        Core::Material lanternBulbMat{};
        lanternBulbMat.textureId = -1;
        lanternBulbMat.roughness = 0.1f;
        lanternBulbMat.metallic  = 0.0f;
        Core::GetAssetManager().addMaterial("lantern_bulb_material", lanternBulbMat);

        // Ground plane
        {
            float xMin = (0         - kGridCols / 2.f) * kSpacingX - kSpacingX * 0.5f;
            float xMax = (kGridCols - kGridCols / 2.f) * kSpacingX - kSpacingX * 0.5f;
            float zMin = (0         - kGridRows / 2.f) * kSpacingZ - kSpacingZ * 0.5f;
            float zMax = (kGridRows - kGridRows / 2.f) * kSpacingZ - kSpacingZ * 0.5f;
            std::vector<Vertex::Mesh> v; std::vector<uint32_t> i;
            HardcodedMeshes::getGroundMesh(v, i, xMin, xMax, zMin, zMax);
            Core::GetAssetManager().addMesh("ground", v, i);
        }

        // Street network
        {
            std::vector<Vertex::Mesh> v; std::vector<uint32_t> i;
            HardcodedMeshes::getStreetNetworkMesh(v, i, kGridCols, kGridRows, kSpacingX, kSpacingZ);
            Core::GetAssetManager().addMesh("street_network", v, i);
        }

        Core::Material groundMat{};
        groundMat.textureId = -1;
        groundMat.roughness = 0.9f;
        groundMat.metallic  = 0.0f;
        Core::GetAssetManager().addMaterial("ground_material", groundMat);

        Core::Material streetMat{};
        streetMat.textureId = -1;
        streetMat.roughness = 0.8f;
        streetMat.metallic  = 0.0f;
        Core::GetAssetManager().addMaterial("street_material", streetMat);
    }

    void setupScene(flecs::world& world) override {

        // Camera
        world.entity("Player")
            .set<Lca::Component::FlyingCamera>({
                .position = glm::vec3(0.0f, 30.0f, 80.0f),
                .angle = Lca::Component::FlyingCamera::lookAt(
                    glm::vec3(0.0f, 30.0f, 80.0f), glm::vec3(0.0f, 0.0f, 0.0f)),
                .speed = 30.0f,
                .rotationSpeed = 0.005f,
                .fov = glm::radians(60.0f),
                .nearClip = 0.1f,
                .farClip = 2000.0f
            })
            .add<Lca::Component::Transform>();

        // IDs
        uint32_t pipelineId    = Core::GetRenderer().getMeshPipelineId("basic");
        uint32_t carMeshId     = Core::GetAssetManager().getMeshId("car");
        uint32_t houseMeshId   = Core::GetAssetManager().getMeshId("house");
        uint32_t lanternPoleMeshId = Core::GetAssetManager().getMeshId("lantern_pole");
        uint32_t lanternBulbRMeshId = Core::GetAssetManager().getMeshId("lantern_bulb_r");
        uint32_t lanternBulbGMeshId = Core::GetAssetManager().getMeshId("lantern_bulb_g");
        uint32_t lanternBulbBMeshId = Core::GetAssetManager().getMeshId("lantern_bulb_b");
        uint32_t carMatId      = Core::GetAssetManager().getMaterialId("car_material");
        uint32_t houseMatId    = Core::GetAssetManager().getMaterialId("house_material");
        uint32_t lanternPoleMatId  = Core::GetAssetManager().getMaterialId("lantern_pole_material");
        uint32_t lanternBulbMatId  = Core::GetAssetManager().getMaterialId("lantern_bulb_material");

        uint32_t groundMeshId  = Core::GetAssetManager().getMeshId("ground");
        uint32_t streetMeshId  = Core::GetAssetManager().getMeshId("street_network");
        uint32_t groundMatId   = Core::GetAssetManager().getMaterialId("ground_material");
        uint32_t streetMatId   = Core::GetAssetManager().getMaterialId("street_material");

        // ── Ground plane ──
        {
            auto e = world.entity("Ground");
            e.set(Component::Transform{
                glm::vec3(0.0f), 0.0f, glm::vec3(0,1,0), glm::vec3(1.0f)
            });
            e.add<Component::TransformID>();
            e.add<Component::Static>();
            auto mesh = world.entity();
            mesh.add(flecs::ChildOf, e);
            mesh.set<Component::Mesh>({groundMeshId, groundMatId, pipelineId});
        }

        // ── Street network ──
        {
            auto e = world.entity("Streets");
            e.set(Component::Transform{
                glm::vec3(0.0f), 0.0f, glm::vec3(0,1,0), glm::vec3(1.0f)
            });
            e.add<Component::TransformID>();
            e.add<Component::Static>();
            auto mesh = world.entity();
            mesh.add(flecs::ChildOf, e);
            mesh.set<Component::Mesh>({streetMeshId, streetMatId, pipelineId});
        }

        // ── Grid layout: 50 cols x 20 rows = 1000 blocks ──
        constexpr float spacingX = kSpacingX;
        constexpr float spacingZ = kSpacingZ;
        constexpr int   gridCols = kGridCols;
        constexpr int   gridRows = kGridRows;

        for (int row = 0; row < gridRows; ++row) {
            for (int col = 0; col < gridCols; ++col) {

                float baseX = (col - gridCols / 2) * spacingX;
                float baseZ = (row - gridRows / 2) * spacingZ;

                // ── House ──
                {
                    auto parent = world.entity();
                    parent.set(Component::Transform{
                        glm::vec3(baseX, 0.0f, baseZ),
                        0.0f, glm::vec3(0,1,0), glm::vec3(1.0f)
                    });
                    parent.add<Component::TransformID>();
                    parent.add<Component::Static>();

                    auto mesh = world.entity();
                    mesh.add(flecs::ChildOf, parent);
                    mesh.set<Component::Mesh>({houseMeshId, houseMatId, pipelineId});
                }

                // ── Car (offset on the street) ──
                {
                    float carAngle = ((row * gridCols + col) % 2 == 0) ? 0.0f : 3.14159f;
                    auto parent = world.entity();
                    parent.set(Component::Transform{
                        glm::vec3(baseX + 5.0f, 0.0f, baseZ + 4.5f),
                        carAngle, glm::vec3(0,1,0), glm::vec3(1.0f)
                    });
                    parent.add<Component::TransformID>();
                    parent.add<Component::Static>();

                    auto mesh = world.entity();
                    mesh.add(flecs::ChildOf, parent);
                    mesh.set<Component::Mesh>({carMeshId, carMatId, pipelineId});
                }

                // ── Street Lantern (roadside) ──
                {
                    auto parent = world.entity();
                    parent.set(Component::Transform{
                        glm::vec3(baseX - 4.0f, 0.0f, baseZ + 5.5f),
                        0.0f, glm::vec3(0,1,0), glm::vec3(1.0f)
                    });
                    parent.add<Component::TransformID>();
                    parent.add<Component::Static>();

                    auto mod = (row * gridCols + col) % 3;
                    glm::vec3 lightColor =
                        (mod == 0) ? glm::vec3(1.0f, 0.0f, 0.0f) :
                        (mod == 1) ? glm::vec3(0.0f, 1.0f, 0.0f) :
                                     glm::vec3(0.0f, 0.0f, 1.0f);

                    uint32_t bulbMeshId =
                        (mod == 0) ? lanternBulbRMeshId :
                        (mod == 1) ? lanternBulbGMeshId :
                                     lanternBulbBMeshId;

                    auto poleMesh = world.entity();
                    poleMesh.add(flecs::ChildOf, parent);
                    poleMesh.set<Component::Mesh>({lanternPoleMeshId, lanternPoleMatId, pipelineId});

                    auto bulbMesh = world.entity();
                    bulbMesh.add(flecs::ChildOf, parent);
                    bulbMesh.set<Component::Mesh>({bulbMeshId, lanternBulbMatId, pipelineId});

                    // Point light at each lantern
                    auto lightEntity = world.entity();
                    lightEntity.set(Component::Transform{
                        glm::vec3(baseX - 4.0f + 1.0f, 4.85f, baseZ + 5.5f),
                        0.0f, glm::vec3(0,1,0), glm::vec3(1.0f)
                    });
                    lightEntity.add<Component::Static>();
                    lightEntity.set(Component::PointLight{
                        .color     = lightColor,
                        .intensity = 100.0f,
                        .radius    = 20.0f
                    });
                }
            }
        }

        // Sun light
        auto sun = world.entity("Sun");
        sun.set(Component::Transform{
            glm::vec3(0.0f, 100.0f, 0.0f),
            glm::radians(-45.0f), glm::vec3(1,1,1), glm::vec3(1.0f)
        });
        // sun.set(Component::DirectionalLight{
        //     .color     = glm::vec3(1.0f, 0.95f, 0.8f),
        //     .intensity = 0.1f
        // });
    }
};
