#pragma once

#include <algorithm>
#include <cmath>
#include <memory>
#include <sstream>
#include <string>

#include <glm/glm.hpp>

class ZoneStreamingLevel;

struct ZoneRegistry {
    static constexpr int   kGridX    = 10;
    static constexpr int   kGridZ    = 10;
    static constexpr int   kZoneRows = 3;
    static constexpr int   kZoneCols = 3;
    static constexpr float kSpacing  = 14.0f;
    static constexpr float kGap      = 10.0f;

    float streamInRadius  = 200.0f;
    float streamOutRadius = 280.0f;

    std::shared_ptr<ZoneStreamingLevel> zones[kGridX * kGridZ];

    std::shared_ptr<ZoneStreamingLevel>& zone(int col, int row) {
        return zones[row * kGridX + col];
    }
};

namespace ZoneHelpers {

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
