#version 460
#extension GL_EXT_nonuniform_qualifier : enable

layout(location = 0) in vec3 inWorldNormal;
layout(location = 1) in vec4 inColor;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) flat in uint inMaterialId;
layout(location = 4) in vec3 inWorldPos;

layout(location = 0) out vec4 outColor;

struct Material {
    int textureId;
    float roughness;
    float metallic;
    float pad0;
    float pad1;
};

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

float saturate(float x) {
    return clamp(x, 0.0, 1.0);
}

void main() {
    Material material = materialBuffer.materials[inMaterialId];

    vec4 texColor = vec4(1.0);
    if (material.textureId >= 0) {
        texColor = texture(textures[nonuniformEXT(material.textureId)], inTexCoord);
    }

    vec3 N = normalize(inWorldNormal);
    vec3 V = normalize(cam.camPos.xyz - inWorldPos);

    // Dark blue base with view-dependent rim to create a shiny glow.
    vec3 deepBlue = vec3(0.01, 0.03, 0.10);
    vec3 neonBlue = vec3(0.12, 0.45, 1.00);

    float rim = pow(1.0 - saturate(dot(N, V)), 2.8);
    float rough = clamp(material.roughness, 0.05, 1.0);
    float metallic = clamp(material.metallic, 0.0, 1.0);

    // Sharper highlight for lower roughness materials.
    vec3 R = reflect(-V, N);
    vec3 L = normalize(vec3(0.35, 0.75, 0.45));
    float specPow = mix(10.0, 90.0, 1.0 - rough);
    float spec = pow(saturate(dot(R, L)), specPow) * mix(0.45, 1.0, metallic);

    vec3 base = deepBlue * (0.55 + inColor.rgb * 0.45);
    vec3 glow = neonBlue * (0.25 + rim * 1.55 + spec * 0.95);

    vec3 color = (base + glow) * texColor.rgb;
    float alpha = saturate((0.55 + rim * 0.45) * inColor.a * texColor.a);

    outColor = vec4(color, alpha);

    if (outColor.a < 0.01) {
        discard;
    }
}
