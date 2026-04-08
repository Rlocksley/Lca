#version 460
#extension GL_EXT_nonuniform_qualifier : enable

layout(location = 0) in vec3 inWorldNormal;
layout(location = 1) in vec4 inColor;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) flat in uint inMaterialId;
layout(location = 4) in vec3 inWorldPos;

layout(location = 0) out vec4 outColor;

// ── Constants ────────────────────────────────────────────────────────
const float PI = 3.14159265359;

const uint LIGHT_TYPE_POINT       = 0;
const uint LIGHT_TYPE_SPOT        = 1;
const uint LIGHT_TYPE_DIRECTIONAL = 2;

// ── Structs ──────────────────────────────────────────────────────────
struct Material {
    int textureId;
    float roughness;
    float metallic;
    float pad0;
    float pad1;
};

struct Light {
    vec4 position;   // xyz = world position, w = type
    vec4 direction;  // xyz = direction (spot/directional), w = unused
    vec4 color;      // rgb = color, a = intensity
    vec4 params;     // x = radius, y = cos(innerCutoff), z = cos(outerCutoff), w = unused
};

// ── Descriptor sets ──────────────────────────────────────────────────
layout(set = 0, binding = 0) uniform CameraBuffer {
    vec4 frustumPlanes[6];
    mat4 projection;
    mat4 view;
    mat4 viewProjection;
    mat4 inverseProjection;
    vec4 camPos;
    vec4 camDir;
    vec2 screenSize;
    float nearClip;
    float farClip;
} cam;

layout(set = 0, binding = 3) readonly buffer MaterialBuffer {
    Material materials[];
} materialBuffer;

layout(set = 0, binding = 4) uniform sampler2D textures[];

layout(std430, set = 0, binding = 5) readonly buffer LightBuffer {
    uint lightCount;
    Light lights[];
} lightData;

layout(std430, set = 0, binding = 6) readonly buffer TileLightIndices {
    uint lightIndices[];
} tileLightIndices;

// ── PBR BRDF Functions ──────────────────────────────────────────────

// Normal Distribution Function — GGX/Trowbridge-Reitz
float distributionGGX(vec3 N, vec3 H, float roughness) {
    float a  = roughness * roughness;
    float a2 = a * a;
    float NdotH  = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float denom = NdotH2 * (a2 - 1.0) + 1.0;
    denom = PI * denom * denom;

    return a2 / max(denom, 0.0000001);
}

// Geometry function — Schlick-GGX (single direction)
float geometrySchlickGGX(float NdotV, float roughness) {
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    return NdotV / (NdotV * (1.0 - k) + k);
}

// Geometry function — Smith's method (both view and light)
float geometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    return geometrySchlickGGX(NdotV, roughness) * geometrySchlickGGX(NdotL, roughness);
}

// Fresnel — Schlick approximation
vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// ── Per-light contribution (Cook-Torrance BRDF) ─────────────────────

vec3 pbrContribution(vec3 L, vec3 radiance, vec3 N, vec3 V,
                     vec3 albedo, float metallic, float roughness, vec3 F0) {
    vec3 H = normalize(V + L);

    float NDF = distributionGGX(N, H, roughness);
    float G   = geometrySmith(N, V, L, roughness);
    vec3  F   = fresnelSchlick(max(dot(H, V), 0.0), F0);

    // Specular (Cook-Torrance)
    vec3  numerator   = NDF * G * F;
    float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
    vec3  specular    = numerator / denominator;

    // Energy conservation: kD + kS = 1  (metals have no diffuse)
    vec3 kS = F;
    vec3 kD = (vec3(1.0) - kS) * (1.0 - metallic);

    float NdotL = max(dot(N, L), 0.0);
    return (kD * albedo / PI + specular) * radiance * NdotL;
}

// ── Light calculators ────────────────────────────────────────────────

vec3 calculatePointLightPBR(Light light, vec3 N, vec3 V, vec3 fragPos,
                            vec3 albedo, float metallic, float roughness, vec3 F0) {
    vec3  L        = light.position.xyz - fragPos;
    float distance = length(L);
    L = normalize(L);

    float radius = light.params.x;
    if (radius > 0.0 && distance > radius) return vec3(0.0);

    // Attenuation (inverse-square with smooth radius falloff)
    float attenuation = 1.0 / (distance * distance + 1.0);
    if (radius > 0.0) {
        float falloff = clamp(1.0 - (distance / radius), 0.0, 1.0);
        attenuation *= falloff * falloff;
    }

    vec3 radiance = light.color.rgb * light.color.a * attenuation;
    return pbrContribution(L, radiance, N, V, albedo, metallic, roughness, F0);
}

vec3 calculateSpotLightPBR(Light light, vec3 N, vec3 V, vec3 fragPos,
                           vec3 albedo, float metallic, float roughness, vec3 F0) {
    vec3  L        = light.position.xyz - fragPos;
    float distance = length(L);
    L = normalize(L);

    // Spot cone
    vec3  spotDir       = normalize(light.direction.xyz);
    float theta         = dot(L, -spotDir);
    float innerCutoff   = light.params.y;
    float outerCutoff   = light.params.z;
    float epsilon       = innerCutoff - outerCutoff;
    float spotIntensity = clamp((theta - outerCutoff) / max(epsilon, 0.001), 0.0, 1.0);

    if (spotIntensity <= 0.0) return vec3(0.0);

    float attenuation = 1.0 / (distance * distance + 1.0);
    float radius = light.params.x;
    if (radius > 0.0) {
        float falloff = clamp(1.0 - (distance / radius), 0.0, 1.0);
        attenuation *= falloff * falloff;
    }

    vec3 radiance = light.color.rgb * light.color.a * attenuation * spotIntensity;
    return pbrContribution(L, radiance, N, V, albedo, metallic, roughness, F0);
}

vec3 calculateDirectionalLightPBR(Light light, vec3 N, vec3 V,
                                  vec3 albedo, float metallic, float roughness, vec3 F0) {
    vec3 L = normalize(-light.direction.xyz);
    vec3 radiance = light.color.rgb * light.color.a;
    return pbrContribution(L, radiance, N, V, albedo, metallic, roughness, F0);
}

// ── Main ─────────────────────────────────────────────────────────────

void main() {
    Material material = materialBuffer.materials[inMaterialId];

    // Sample albedo texture (sRGB → linear handled by the sampler/format)
    vec4 texColor = vec4(1.0);
    if (material.textureId >= 0) {
        texColor = texture(textures[nonuniformEXT(material.textureId)], inTexCoord);
    }

    vec3  albedo    = inColor.rgb * texColor.rgb;
    float metallic  = clamp(material.metallic, 0.0, 1.0);
    float roughness = clamp(material.roughness, 0.04, 1.0);  // 0.04 minimum avoids div-by-zero artefacts

    vec3 N = normalize(inWorldNormal);
    vec3 V = normalize(cam.camPos.xyz - inWorldPos);

    // Dielectrics reflect ~4% at normal incidence; metals use albedo as F0
    vec3 F0 = mix(vec3(0.04), albedo, metallic);

    // Ambient term (IBL placeholder — uniform ambient for now)
    vec3 ambient = vec3(0.03) * albedo;
    vec3 Lo = vec3(0.0);

    // ── Tiled forward: only iterate lights assigned to this tile ─────
    const uint TILE_SIZE           = 16u;
    const uint MAX_LIGHTS_PER_TILE = 256u;

    uint px     = uint(gl_FragCoord.x);
    uint py     = uint(gl_FragCoord.y);
    uint tilesX = uint((cam.screenSize.x + float(TILE_SIZE) - 1.0) / float(TILE_SIZE));
    uint tileX  = px / TILE_SIZE;
    uint tileY  = py / TILE_SIZE;
    uint tid    = tileY * tilesX + tileX;

    uint entriesPerTile = MAX_LIGHTS_PER_TILE + 1u;
    uint base  = tid * entriesPerTile;
    uint count = tileLightIndices.lightIndices[base];

    for (uint li = 0u; li < count; ++li) {
        uint lightIdx = tileLightIndices.lightIndices[base + 1u + li];
        if (lightIdx >= lightData.lightCount) continue;

        Light light     = lightData.lights[lightIdx];
        uint  lightType = uint(round(light.position.w));

        if (lightType == LIGHT_TYPE_POINT) {
            Lo += calculatePointLightPBR(light, N, V, inWorldPos, albedo, metallic, roughness, F0);
        } else if (lightType == LIGHT_TYPE_SPOT) {
            Lo += calculateSpotLightPBR(light, N, V, inWorldPos, albedo, metallic, roughness, F0);
        } else if (lightType == LIGHT_TYPE_DIRECTIONAL) {
            Lo += calculateDirectionalLightPBR(light, N, V, albedo, metallic, roughness, F0);
        }
    }

    // Fallback so the scene isn't black with zero lights
    if (lightData.lightCount == 0) {
        vec3 defaultDir = normalize(vec3(0.4, 0.7, 0.2));
        vec3 radiance   = vec3(1.0);
        Lo = pbrContribution(defaultDir, radiance, N, V, albedo, metallic, roughness, F0);
    }

    vec3 color = ambient + Lo;

    // HDR tonemapping (Reinhard) + gamma correction
    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0 / 2.2));

    outColor = vec4(color, inColor.a * texColor.a);
}
