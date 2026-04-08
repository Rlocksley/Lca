#version 460
#extension GL_EXT_nonuniform_qualifier : enable

// particle_fire.frag — fire particle fragment shader.
// Samples a procedural fire sprite texture; uses the particle's alpha tint
// for lifecycle fade-in/fade-out.  No PBR lighting: fire is self-luminous.

layout(location = 0) in vec3 inWorldNormal;
layout(location = 1) in vec4 inColor;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) flat in uint inMaterialId;
layout(location = 4) in vec3 inWorldPos;

layout(location = 0) out vec4 outColor;

// ── Structs ───────────────────────────────────────────────────────────────────

struct Material {
    int   textureId;
    float roughness;
    float metallic;
    float pad0;
    float pad1;
};

// ── Descriptor bindings ───────────────────────────────────────────────────────

layout(set = 0, binding = 3) readonly buffer MaterialBuffer {
    Material materials[];
} materialBuffer;

layout(set = 0, binding = 4) uniform sampler2D textures[];

float saturate(float x) {
    return clamp(x, 0.0, 1.0);
}

vec2 saturate(vec2 x) {
    return clamp(x, vec2(0.0), vec2(1.0));
}

vec3 saturate(vec3 x) {
    return clamp(x, vec3(0.0), vec3(1.0));
}

vec4 saturate(vec4 x) {
    return clamp(x, vec4(0.0), vec4(1.0));
}

vec3 flameRamp(float t) {
    vec3 ember  = vec3(0.22, 0.02, 0.00);
    vec3 orange = vec3(1.00, 0.36, 0.05);
    vec3 yellow = vec3(1.00, 0.84, 0.34);
    vec3 white  = vec3(1.00, 0.98, 0.90);

    vec3 c = mix(ember, orange, smoothstep(0.05, 0.45, t));
    c = mix(c, yellow, smoothstep(0.35, 0.78, t));
    c = mix(c, white,  smoothstep(0.72, 1.00, t));
    return c;
}

// ── Main ──────────────────────────────────────────────────────────────────────
void main() {
    Material mat = materialBuffer.materials[inMaterialId];

    vec2 uv = inTexCoord;
    vec2 p = uv * 2.0 - 1.0;

    // Screen-space-free pseudo flicker from world position gives each particle
    // a distinct internal turbulence pattern without time uniforms.
    float flicker = fract(sin(dot(inWorldPos.xy + inWorldPos.zx, vec2(17.13, 31.77))) * 43758.5453);
    float warp = (flicker - 0.5) * 0.16;
    uv.x += warp * (0.25 + uv.y * 0.75);
    uv.y += (flicker - 0.5) * 0.04;

    vec4 fireTexel = vec4(0.0);
    if (mat.textureId >= 0) {
        fireTexel = texture(textures[nonuniformEXT(mat.textureId)], uv);
    }

    // Additional radial attenuation ensures no visible square corners even if
    // the texture mip level softens alpha too much.
    float r = length(p);
    float radialMask = 1.0 - smoothstep(0.62, 1.00, r);

    float texHeat = saturate(fireTexel.r);
    float texAlpha = saturate(fireTexel.a) * radialMask;

    // Shape brightness with centre-hot profile and slight edge ember glow.
    float core = saturate(1.0 - r * 1.45);
    float emberEdge = smoothstep(0.55, 0.95, texHeat) * smoothstep(0.95, 0.55, r);
    float heat = saturate(texHeat * (0.75 + core * 0.65) + emberEdge * 0.22);

    // Lifecycle tint from simulation still drives hot->cool evolution.
    float lifeHeat = dot(saturate(inColor.rgb), vec3(0.28, 0.59, 0.13));
    vec3 baseFlame = flameRamp(heat * (0.65 + lifeHeat * 0.55));
    vec3 color = baseFlame * (0.55 + inColor.rgb * 1.35);

    float alpha = texAlpha * inColor.a;
    // Small boost on bright regions creates a soft emissive fringe.
    alpha = saturate(alpha + heat * 0.10 * inColor.a);

    outColor = vec4(color, alpha);

    // Discard nearly transparent fragments to avoid z-sorting artefacts
    if (outColor.a < 0.01) discard;
}
