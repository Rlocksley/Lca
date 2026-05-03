#version 460
#extension GL_EXT_nonuniform_qualifier : enable

// fireball.frag — fireball particle fragment shader.
// Self-luminous fiery particles that envelop the projectile sphere.
// Uses the fire sprite texture with a hot-core colour ramp,
// but tinted by the particle's lifecycle colour from the sim shader.

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

// ── Helpers ───────────────────────────────────────────────────────────────────

float saturate(float x) { return clamp(x, 0.0, 1.0); }
vec3  saturate(vec3 x)  { return clamp(x, vec3(0.0), vec3(1.0)); }

// Fire colour ramp: ember → orange → yellow → white
vec3 fireRamp(float t) {
    vec3 ember  = vec3(0.25, 0.03, 0.00);
    vec3 orange = vec3(1.00, 0.40, 0.06);
    vec3 yellow = vec3(1.00, 0.82, 0.30);
    vec3 white  = vec3(1.00, 0.97, 0.88);

    vec3 c = mix(ember, orange, smoothstep(0.05, 0.40, t));
    c = mix(c, yellow, smoothstep(0.30, 0.75, t));
    c = mix(c, white,  smoothstep(0.70, 1.00, t));
    return c;
}

// ── Main ──────────────────────────────────────────────────────────────────────
void main() {
    Material mat = materialBuffer.materials[inMaterialId];

    vec2 uv = inTexCoord;
    vec2 p  = uv * 2.0 - 1.0;

    // Per-particle pseudo flicker from world position
    float flicker = fract(sin(dot(inWorldPos.xy + inWorldPos.zx, vec2(17.13, 31.77))) * 43758.5453);
    float warp = (flicker - 0.5) * 0.20;
    uv.x += warp * (0.3 + uv.y * 0.7);
    uv.y += (flicker - 0.5) * 0.06;

    // Sample fire sprite texture
    vec4 fireTexel = vec4(0.0);
    if (mat.textureId >= 0) {
        fireTexel = texture(textures[nonuniformEXT(mat.textureId)], uv);
    }

    // Radial mask — soft Gaussian-like blob kills square edges completely
    float r = length(p);
    float radialMask = 1.0 - smoothstep(0.0, 0.75, r);
    radialMask *= radialMask;  // squared falloff → smooth blob, zero well before corners

    float texHeat  = saturate(fireTexel.r);
    float texAlpha = saturate(fireTexel.a) * radialMask;

    // Central hot core with ember glow at edges
    float core = saturate(1.0 - r * 1.4);
    float emberEdge = smoothstep(0.40, 0.70, texHeat) * smoothstep(0.70, 0.30, r);
    float heat = saturate(texHeat * (0.80 + core * 0.80) + emberEdge * 0.35);

    // Lifecycle tint from the simulation (hot → cool as particle ages)
    float lifeHeat = dot(saturate(inColor.rgb), vec3(0.28, 0.59, 0.13));
    vec3 baseFlame = fireRamp(heat * (0.55 + lifeHeat * 0.65));

    // Boost intensity — fireballs should be bright and vivid
    vec3 color = baseFlame * (1.2 + inColor.rgb * 2.0);

    float alpha = texAlpha * inColor.a;
    // Emissive fringe for bright regions
    alpha = saturate(alpha + heat * 0.22 * inColor.a);

    outColor = vec4(color, alpha);

    if (outColor.a < 0.01) discard;
}
