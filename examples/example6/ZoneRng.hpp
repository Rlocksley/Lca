#pragma once

#include <cstdint>

#include <glm/glm.hpp>

struct ZoneRng {
    uint32_t state;
    explicit ZoneRng(uint32_t seed) : state(seed ? seed : 1u) {}

    uint32_t next() {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return state;
    }

    float nextFloat() {
        return (next() & 0x7FFFFFFF) / static_cast<float>(0x7FFFFFFF);
    }

    float nextFloat(float lo, float hi) {
        return lo + nextFloat() * (hi - lo);
    }

    int nextInt(int lo, int hi) {
        if (lo >= hi) return lo;
        return lo + static_cast<int>(next() % static_cast<uint32_t>(hi - lo + 1));
    }

    bool nextBool(float p = 0.5f) { return nextFloat() < p; }

    glm::vec4 varyColor(glm::vec4 base, float amount = 0.12f) {
        return glm::vec4(
            glm::clamp(base.r + nextFloat(-amount, amount), 0.0f, 1.0f),
            glm::clamp(base.g + nextFloat(-amount, amount), 0.0f, 1.0f),
            glm::clamp(base.b + nextFloat(-amount, amount), 0.0f, 1.0f),
            1.0f);
    }
};
