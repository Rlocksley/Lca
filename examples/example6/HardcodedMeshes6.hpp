#pragma once

#include "Vertex.hpp"

#include <algorithm>
#include <cstdint>
#include <vector>

using namespace Lca;

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

    struct BuildingParams {
        float width      = 2.5f;
        float height     = 4.0f;
        float depth      = 2.5f;
        float roofHeight = 2.0f;
        int   windowRows = 0;
        int   windowCols = 0;
        bool  hasAntenna = false;
        glm::vec4 wallCol   = glm::vec4(0.8f, 0.8f, 0.7f, 1.0f);
        glm::vec4 roofCol   = glm::vec4(0.5f, 0.25f, 0.1f, 1.0f);
        glm::vec4 doorCol   = glm::vec4(0.3f, 0.15f, 0.05f, 1.0f);
        glm::vec4 windowCol = glm::vec4(0.55f, 0.78f, 1.0f, 0.9f);
    };

    inline void getBuildingMesh(std::vector<V>& verts, std::vector<uint32_t>& idx,
                                const BuildingParams& p) {
        verts.clear(); idx.clear();
        MeshBuilder mb{verts, idx};

        float wx = p.width, wy = p.height, wz = p.depth;
        mb.addBox({-wx, 0, -wz}, {wx, wy, wz}, p.wallCol);

        float dw = std::min(0.6f, wx * 0.4f);
        float dh = std::min(2.0f, wy * 0.45f);
        float dz = wz + 0.01f;
        mb.addQuad({dw, 0, dz}, {-dw, 0, dz}, {-dw, dh, dz}, {dw, dh, dz}, p.doorCol);

        if (p.windowRows > 0 && p.windowCols > 0) {
            float winW = std::min(0.45f, wx * 0.55f / p.windowCols);
            float winH = std::min(0.65f, (wy - dh) * 0.65f / std::max(1, p.windowRows));
            float startY = dh + 0.5f;
            float spacingY = (wy - startY) / (p.windowRows + 0.5f);

            {
                float spacingX = (wx * 2.0f) / (p.windowCols + 1);
                for (int face = 0; face < 2; ++face) {
                    float fz = (face == 0) ? (wz + 0.02f) : (-wz - 0.02f);
                    for (int r = 0; r < p.windowRows; ++r) {
                        float cy = startY + spacingY * (r + 0.5f);
                        for (int c = 0; c < p.windowCols; ++c) {
                            float cx = -wx + spacingX * (c + 1);
                            if (face == 0) {
                                mb.addQuad({cx + winW, cy - winH, fz}, {cx - winW, cy - winH, fz},
                                           {cx - winW, cy + winH, fz}, {cx + winW, cy + winH, fz}, p.windowCol);
                            } else {
                                mb.addQuad({cx - winW, cy - winH, fz}, {cx + winW, cy - winH, fz},
                                           {cx + winW, cy + winH, fz}, {cx - winW, cy + winH, fz}, p.windowCol);
                            }
                        }
                    }
                }
            }

            {
                int sideCols = std::max(1, p.windowCols - 1);
                float spacingZ = (wz * 2.0f) / (sideCols + 1);
                for (int face = 0; face < 2; ++face) {
                    float fx = (face == 0) ? (wx + 0.02f) : (-wx - 0.02f);
                    for (int r = 0; r < p.windowRows; ++r) {
                        float cy = startY + spacingY * (r + 0.5f);
                        for (int c = 0; c < sideCols; ++c) {
                            float cz = -wz + spacingZ * (c + 1);
                            if (face == 0) {
                                mb.addQuad({fx, cy - winH, cz - winW}, {fx, cy - winH, cz + winW},
                                           {fx, cy + winH, cz + winW}, {fx, cy + winH, cz - winW}, p.windowCol);
                            } else {
                                mb.addQuad({fx, cy - winH, cz + winW}, {fx, cy - winH, cz - winW},
                                           {fx, cy + winH, cz - winW}, {fx, cy + winH, cz + winW}, p.windowCol);
                            }
                        }
                    }
                }
            }
        }

        if (p.roofHeight > 0.01f) {
            float rh = p.roofHeight, overhang = 0.3f;
            float rx = wx + overhang, rz = wz + overhang;
            mb.addQuad({-rx, wy, -rz}, {rx, wy, -rz}, {rx, wy + rh, 0}, {-rx, wy + rh, 0}, p.roofCol);
            mb.addQuad({rx, wy, rz}, {-rx, wy, rz}, {-rx, wy + rh, 0}, {rx, wy + rh, 0}, p.roofCol);
            mb.addTri({rx, wy, -rz}, {rx, wy, rz}, {rx, wy + rh, 0}, p.roofCol);
            mb.addTri({-rx, wy, rz}, {-rx, wy, -rz}, {-rx, wy + rh, 0}, p.roofCol);
        } else {
            float capH = 0.15f;
            mb.addBox({-wx - 0.1f, wy, -wz - 0.1f}, {wx + 0.1f, wy + capH, wz + 0.1f}, p.roofCol);
        }

        if (p.hasAntenna) {
            float ah = 3.0f, aw = 0.05f;
            float topY = (p.roofHeight > 0.01f) ? (wy + p.roofHeight) : (wy + 0.15f);
            mb.addBox({-aw, topY, -aw}, {aw, topY + ah, aw}, glm::vec4(0.3f, 0.3f, 0.3f, 1.0f));
        }
    }

    struct CarParams {
        float bodyLength  = 2.0f;
        float bodyHeight  = 1.2f;
        float bodyWidth   = 1.0f;
        float cabinStart  = -0.5f;
        float cabinEnd    = 1.5f;
        float cabinHeight = 1.0f;
        bool  hasBed      = false;
        glm::vec4 bodyCol = glm::vec4(0.7f, 0.2f, 0.2f, 1.0f);
    };

    inline void getParametricCarMesh(std::vector<V>& verts, std::vector<uint32_t>& idx,
                                      const CarParams& p) {
        verts.clear(); idx.clear();
        MeshBuilder mb{verts, idx};

        float bx = p.bodyLength, by = p.bodyHeight, bz = p.bodyWidth;
        mb.addBox({-bx, 0, -bz}, {bx, by, bz}, p.bodyCol);

        glm::vec4 glassCol(0.6f, 0.8f, 1.0f, 1.0f);
        float cz = bz * 0.9f;
        mb.addBox({p.cabinStart, by, -cz}, {p.cabinEnd, by + p.cabinHeight, cz}, glassCol);

        if (p.hasBed) {
            float bedStart = -bx;
            float bedEnd   = p.cabinStart - 0.1f;
            float bedH     = by * 0.4f;
            glm::vec4 bedCol(p.bodyCol.r * 0.85f, p.bodyCol.g * 0.85f, p.bodyCol.b * 0.85f, 1.0f);
            mb.addBox({bedStart, by, -bz - 0.05f}, {bedEnd, by + bedH, -bz + 0.15f}, bedCol);
            mb.addBox({bedStart, by,  bz - 0.15f}, {bedEnd, by + bedH,  bz + 0.05f}, bedCol);
            mb.addBox({bedStart - 0.05f, by, -bz}, {bedStart + 0.15f, by + bedH, bz}, bedCol);
        }

        glm::vec4 wheelCol(0.15f, 0.15f, 0.15f, 1.0f);
        float wr = 0.35f;
        float wlx1 = bx * 0.65f, wlx2 = -bx * 0.65f, wlz = bz + 0.1f;
        struct WP { float x, z; };
        WP wheels[] = {{wlx1, wlz}, {wlx1, -wlz}, {wlx2, wlz}, {wlx2, -wlz}};
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
        float hw = 3.0f;

        float zMid = (zMin + zMax) * 0.5f;
        mb.addQuad({xMin, y, zMin - hw}, {xMax, y, zMin - hw}, {xMax, y, zMin + hw}, {xMin, y, zMin + hw}, roadCol);
        mb.addQuad({xMin, y, zMax - hw}, {xMax, y, zMax - hw}, {xMax, y, zMax + hw}, {xMin, y, zMax + hw}, roadCol);
        mb.addQuad({xMin, y, zMid - hw}, {xMax, y, zMid - hw}, {xMax, y, zMid + hw}, {xMin, y, zMid + hw}, roadCol);

        float xMid = (xMin + xMax) * 0.5f;
        mb.addQuad({xMin - hw, y, zMin}, {xMin + hw, y, zMin}, {xMin + hw, y, zMax}, {xMin - hw, y, zMax}, roadCol);
        mb.addQuad({xMax - hw, y, zMin}, {xMax + hw, y, zMin}, {xMax + hw, y, zMax}, {xMax - hw, y, zMax}, roadCol);
        mb.addQuad({xMid - hw, y, zMin}, {xMid + hw, y, zMin}, {xMid + hw, y, zMax}, {xMid - hw, y, zMax}, roadCol);
    }

} // namespace HardcodedMeshes6
