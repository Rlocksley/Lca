#pragma once

#include <cmath>
#include <cstdint>
#include <vector>

namespace ProceduralTex {

    inline float hash(float x, float y) {
        float v = std::sin(x * 127.1f + y * 311.7f) * 43758.5453f;
        return v - std::floor(v);
    }

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

    inline std::vector<uint32_t> generateFire(int w, int h, float seed) {
        std::vector<uint32_t> pixels(w * h);
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                float u = static_cast<float>(x) / w;
                float v = static_cast<float>(y) / h;

                float gradient = 1.0f - v;
                float n = fbm(u * 6.0f + seed, v * 6.0f + seed * 0.7f, 5);
                float flame = gradient * 0.7f + n * 0.5f;
                flame = std::fmax(0.0f, std::fmin(1.0f, flame));

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

    inline std::vector<uint32_t> generateSky(int w, int h, float seed) {
        std::vector<uint32_t> pixels(w * h);
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                float u = static_cast<float>(x) / w;
                float v = static_cast<float>(y) / h;
                float n = fbm(u * 6.0f + seed, v * 4.0f + seed * 0.5f, 5);
                float t = std::fmin(1.0f, v * 2.0f);
                float r = 0.15f + 0.45f * t;
                float g = 0.30f + 0.45f * t;
                float b = 0.80f + 0.15f * t;
                float cloudBand = 1.0f - std::fabs(v - 0.45f) * 4.0f;
                cloudBand = std::fmax(0.0f, cloudBand);
                float cloud = std::fmax(0.0f, n - 0.45f) * 3.0f * cloudBand;
                cloud = std::fmin(1.0f, cloud);
                r += cloud * (1.0f - r);
                g += cloud * (1.0f - g);
                b += cloud * (1.0f - b);
                r = std::fmax(0.0f, std::fmin(1.0f, r));
                g = std::fmax(0.0f, std::fmin(1.0f, g));
                b = std::fmax(0.0f, std::fmin(1.0f, b));
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
