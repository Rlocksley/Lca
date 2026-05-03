#version 460
#extension GL_EXT_nonuniform_qualifier : enable

// fireball_ember.frag — Bright point-like fragment shader for ember / spark
// particles.  No texture sampling — just a soft radial dot with the hot
// colour supplied by the compute simulation.

layout(location = 0) in vec3 inWorldNormal;
layout(location = 1) in vec4 inColor;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) flat in uint inMaterialId;
layout(location = 4) in vec3 inWorldPos;

layout(location = 0) out vec4 outColor;

// ── Descriptor bindings (must match ParticleSystemPipeline layout) ─────────────

struct Material {
    int   textureId;
    float roughness;
    float metallic;
    float pad0;
    float pad1;
};

layout(set = 0, binding = 3) readonly buffer MaterialBuffer {
    Material materials[];
} materialBuffer;

layout(set = 0, binding = 4) uniform sampler2D textures[];

// ── Main ──────────────────────────────────────────────────────────────────────
void main() {
    vec2  p = inTexCoord * 2.0 - 1.0;
    float r = length(p);

    // Soft circular dot: bright core, Gaussian-like falloff
    float dot_ = 1.0 - smoothstep(0.0, 0.65, r);
    dot_ = dot_ * dot_ * dot_; // cubed for a tight hot point, zero at edges

    // Bright emissive colour from the simulation
    vec3  color = inColor.rgb * (2.0 + dot_ * 4.0);
    float alpha = dot_ * inColor.a;

    outColor = vec4(color, alpha);

    if (outColor.a < 0.01) discard;
}
