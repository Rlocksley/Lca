#pragma once

#include <cmath>
#include <cstdint>
#include <vector>

// Procedural texture generators for example9.
// All functions return a vector of packed R8G8B8A8 uint32_t pixels in
// row-major order (row 0 = top), compatible with VK_FORMAT_R8G8B8A8_SRGB.
// Pack as: (A<<24) | (B<<16) | (G<<8) | R  — little-endian RGBA in memory.

namespace ProceduralTex {

    static const float kPi = 3.14159265359f;

    inline float hash(float x, float y) {
        float v = std::sin(x * 127.1f + y * 311.7f) * 43758.5453f;
        return v - std::floor(v);
    }

    inline float noise(float x, float y) {
        float ix = std::floor(x), iy = std::floor(y);
        float fx = x - ix,        fy = y - iy;
        float a = hash(ix,       iy);
        float b = hash(ix + 1.f, iy);
        float c = hash(ix,       iy + 1.f);
        float d = hash(ix + 1.f, iy + 1.f);
        float ux = fx * fx * (3.f - 2.f * fx);
        float uy = fy * fy * (3.f - 2.f * fy);
        return a + (b - a) * ux + (c - a) * uy + (a - b - c + d) * ux * uy;
    }

    inline float fbm(float x, float y, int octaves = 5) {
        float val = 0.f, amp = 0.5f;
        for (int i = 0; i < octaves; ++i) {
            val += amp * noise(x, y);
            x   *= 2.f; y   *= 2.f;
            amp *= 0.5f;
        }
        return val;
    }

    inline uint8_t clamp8(float v) {
        int i = static_cast<int>(v * 255.f);
        return static_cast<uint8_t>(i < 0 ? 0 : (i > 255 ? 255 : i));
    }

    // Generates an anisotropic flame sprite with noisy tongues and a soft halo.
    // R = G = B = luminance mask used as emissive strength in the fragment shader.
    // A = shaped flame alpha that strongly rejects quad corners.
    //   w, h   — texture dimensions (power-of-two recommended, e.g. 128x128)
    //   seed   — float seed shifts the noise pattern (vary per emitter)
    inline std::vector<uint32_t> generateFireSprite(int w, int h, float seed) {
        std::vector<uint32_t> pixels(w * h);

        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                // Sprite-space UV in [-1, 1], with +y as flame-up direction.
                float u = (static_cast<float>(x) + 0.5f) / static_cast<float>(w) * 2.f - 1.f;
                float v = (static_cast<float>(y) + 0.5f) / static_cast<float>(h) * 2.f - 1.f;
                float y01 = (v + 1.0f) * 0.5f;

                // Build directional noises: broad flow + finer breakup.
                float flow = fbm((u + seed * 0.7f) * 2.6f, (y01 + seed * 0.3f) * 3.4f, 4);
                float detail = fbm((u + seed * 1.9f) * 7.8f, (y01 + seed * 0.9f) * 11.2f, 3);

                // Width tapers as flame rises. Bottom is wider and denser.
                float baseWidth = 0.70f;
                float tipWidth  = 0.12f;
                float width = baseWidth + (tipWidth - baseWidth) * y01;

                // Turbulent horizontal displacement grows toward the tip.
                float swayAmp = 0.06f + y01 * 0.18f;
                float displacedU = u + (flow - 0.5f) * swayAmp;

                // Elliptical distance around a rising axis-aligned plume.
                float coreDist = std::fabs(displacedU) / (width + 1e-4f);

                // Add tongue-like edge spikes with angular modulation.
                float ang = std::atan2(v, displacedU + 1e-5f);
                float tongues = std::sin(ang * 6.0f + detail * kPi * 2.0f) * 0.08f;
                coreDist += tongues * (0.2f + y01 * 0.8f);

                // Vertical falloff keeps bottom thick and top wispy.
                float verticalShape = 1.0f - y01 * y01 * 0.85f;
                verticalShape = verticalShape < 0.0f ? 0.0f : verticalShape;

                // Flame body alpha with noisy breakup.
                float body = 1.0f - coreDist;
                body = body < 0.0f ? 0.0f : (body > 1.0f ? 1.0f : body);
                body = body * body * (3.0f - 2.0f * body);
                float breakup = (detail - 0.5f) * (0.16f + y01 * 0.14f);
                float alpha = body * verticalShape + breakup;

                // Suppress square corners with soft radial attenuation.
                float rr = std::sqrt(u * u + v * v);
                float cornerFade = 1.0f - (rr - 0.65f) / 0.55f;
                cornerFade = cornerFade < 0.0f ? 0.0f : (cornerFade > 1.0f ? 1.0f : cornerFade);
                alpha *= cornerFade;
                alpha = alpha < 0.0f ? 0.0f : (alpha > 1.0f ? 1.0f : alpha);

                // Luminance: very hot lower core, cooler outer/tip regions.
                float hotCore = 1.0f - coreDist * 1.65f;
                hotCore = hotCore < 0.0f ? 0.0f : (hotCore > 1.0f ? 1.0f : hotCore);
                float baseBoost = 1.0f - y01;
                float heat = hotCore * (0.55f + baseBoost * 0.65f) + (flow - 0.5f) * 0.12f;
                heat = heat < 0.0f ? 0.0f : (heat > 1.0f ? 1.0f : heat);
                heat *= alpha;

                uint32_t px =
                    (static_cast<uint32_t>(clamp8(alpha)) << 24) |
                    (static_cast<uint32_t>(clamp8(heat))  << 16) |
                    (static_cast<uint32_t>(clamp8(heat))  <<  8) |
                     static_cast<uint32_t>(clamp8(heat));
                pixels[y * w + x] = px;
            }
        }
        return pixels;
    }

} // namespace ProceduralTex
