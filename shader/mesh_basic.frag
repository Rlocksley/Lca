#version 460
#extension GL_EXT_nonuniform_qualifier : enable

layout(location = 0) in vec3 inWorldNormal;
layout(location = 1) in vec4 inColor;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) flat in uint inMaterialId;
layout(location = 4) in vec3 inWorldPos;

layout(location = 0) out vec4 outColor;

// Light types
const uint LIGHT_TYPE_POINT       = 0;
const uint LIGHT_TYPE_SPOT        = 1;
const uint LIGHT_TYPE_DIRECTIONAL = 2;

struct Material {
    int textureId;
    float roughness;
    float metallic;
    float pad0;
    float pad1;
};

struct Light {
    vec4 position;   // xyz = world position, w = type (0=point, 1=spot, 2=directional)
    vec4 direction;  // xyz = direction (spot/directional), w = unused
    vec4 color;      // rgb = color, a = intensity
    vec4 params;     // x = radius, y = cos(innerCutoff), z = cos(outerCutoff), w = unused
};

layout(set = 0, binding = 0) uniform CameraBuffer {
    vec4 frustumPlanes[6];
    mat4 projection;       // Projection matrix
    mat4 view;             // View matrix (world → view)
    mat4 viewProjection;
    mat4 inverseProjection;// Inverse of projection
    vec4 camPos;
    vec4 camDir;
    vec2 screenSize;       // Width, height in pixels
    float nearClip;        // Near plane distance
    float farClip;         // Far plane distance
} cam;

layout(set = 0, binding = 3) readonly buffer MaterialBuffer {
    Material materials[];
} materialBuffer;

layout(set = 0, binding = 4) uniform sampler2D textures[];

layout(std430, set = 0, binding = 5) readonly buffer LightBuffer {
    uint lightCount;
    Light lights[];
} lightData;

// Tile light indices written by the light cull compute shader.
// Layout: lightIndices[ tid * (MAX_LIGHTS_PER_TILE + 1) + 0 ] = count
//         lightIndices[ tid * (MAX_LIGHTS_PER_TILE + 1) + 1.. ] = light indices
layout(std430, set = 0, binding = 6) readonly buffer TileLightIndices {
    uint lightIndices[];
} tileLightIndices;

vec3 calculatePointLight(Light light, vec3 N, vec3 V, vec3 fragPos) {
    vec3 L = light.position.xyz - fragPos;
    float distance = length(L);
    L = normalize(L);

    float radius = light.params.x;
    if (radius > 0.0 && distance > radius) return vec3(0.0);

    // Attenuation
    float attenuation = 1.0 / (1.0 + distance * distance);
    if (radius > 0.0) {
        float falloff = clamp(1.0 - (distance / radius), 0.0, 1.0);
        attenuation *= falloff * falloff;
    }

    // Diffuse
    float NdotL = max(dot(N, L), 0.0);
    vec3 diffuse = NdotL * light.color.rgb * light.color.a;

    // Specular (Blinn-Phong)
    vec3 H = normalize(L + V);
    float NdotH = max(dot(N, H), 0.0);
    vec3 specular = pow(NdotH, 32.0) * light.color.rgb * light.color.a;

    return (diffuse + specular) * attenuation;
}

vec3 calculateSpotLight(Light light, vec3 N, vec3 V, vec3 fragPos) {
    vec3 L = light.position.xyz - fragPos;
    float distance = length(L);
    L = normalize(L);

    // Spot cone
    vec3 spotDir = normalize(light.direction.xyz);
    float theta = dot(L, -spotDir);
    float innerCutoff = light.params.y;
    float outerCutoff = light.params.z;

    float epsilon = innerCutoff - outerCutoff;
    float spotIntensity = clamp((theta - outerCutoff) / max(epsilon, 0.001), 0.0, 1.0);

    if (spotIntensity <= 0.0) return vec3(0.0);

    // Attenuation
    float attenuation = 1.0 / (1.0 + distance * distance);
    float radius = light.params.x;
    if (radius > 0.0) {
        float falloff = clamp(1.0 - (distance / radius), 0.0, 1.0);
        attenuation *= falloff * falloff;
    }

    // Diffuse
    float NdotL = max(dot(N, L), 0.0);
    vec3 diffuse = NdotL * light.color.rgb * light.color.a;

    // Specular (Blinn-Phong)
    vec3 H = normalize(L + V);
    float NdotH = max(dot(N, H), 0.0);
    vec3 specular = pow(NdotH, 32.0) * light.color.rgb * light.color.a;

    return (diffuse + specular) * attenuation * spotIntensity;
}

vec3 calculateDirectionalLight(Light light, vec3 N, vec3 V) {
    vec3 L = normalize(-light.direction.xyz);

    // Diffuse
    float NdotL = max(dot(N, L), 0.0);
    vec3 diffuse = NdotL * light.color.rgb * light.color.a;

    // Specular (Blinn-Phong)
    vec3 H = normalize(L + V);
    float NdotH = max(dot(N, H), 0.0);
    vec3 specular = pow(NdotH, 32.0) * light.color.rgb * light.color.a;

    return diffuse + specular;
}

void main() {
    Material material = materialBuffer.materials[inMaterialId];

    vec4 texColor = vec4(1.0);
    if (material.textureId >= 0) {
        texColor = texture(textures[nonuniformEXT(material.textureId)], inTexCoord);
    }

    vec3 N = normalize(inWorldNormal);
    vec3 V = normalize(cam.camPos.xyz - inWorldPos);

    // Ambient
    vec3 ambient = vec3(0.03);
    vec3 lighting = ambient;

    // Accumulate lighting from culled lights using the per-tile light index list
    const uint TILE_SIZE = 16u;
    const uint MAX_LIGHTS_PER_TILE = 256u;

    // Compute tile coordinates from fragment position
    uint px = uint(gl_FragCoord.x);
    uint py = uint(gl_FragCoord.y);
    uint tilesX = uint((cam.screenSize.x + float(TILE_SIZE) - 1.0) / float(TILE_SIZE));
    uint tileX = px / TILE_SIZE;
    uint tileY = py / TILE_SIZE;
    uint tid = tileY * tilesX + tileX;

    uint entriesPerTile = MAX_LIGHTS_PER_TILE + 1u; // count + indices
    uint base = tid * entriesPerTile;
    uint count = tileLightIndices.lightIndices[base];

    for (uint li = 0u; li < count; ++li) {
        uint lightIdx = tileLightIndices.lightIndices[base + 1u + li];
        if (lightIdx >= lightData.lightCount) continue;
        Light light = lightData.lights[lightIdx];
        uint lightType = uint(round(light.position.w));

        if (lightType == LIGHT_TYPE_POINT) {
            lighting += calculatePointLight(light, N, V, inWorldPos);
        } else if (lightType == LIGHT_TYPE_SPOT) {
            lighting += calculateSpotLight(light, N, V, inWorldPos);
        } else if (lightType == LIGHT_TYPE_DIRECTIONAL) {
            lighting += calculateDirectionalLight(light, N, V);
        }
    }

    // Fallback when no lights exist so scene isn't black
    if (lightData.lightCount == 0) {
        vec3 defaultDir = normalize(vec3(0.4, 0.7, 0.2));
        float NdotL = max(dot(N, defaultDir), 0.1);
        lighting = vec3(NdotL);
    }

    outColor = vec4(inColor.rgb * texColor.rgb * lighting, inColor.a * texColor.a);
}
