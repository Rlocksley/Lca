#version 460
#extension GL_EXT_nonuniform_qualifier : enable

layout(location = 0) in vec2 inTexCoord;
layout(location = 1) flat in uint inMaterialId;
layout(location = 2) flat in vec4 inClipRect;

layout(location = 0) out vec4 outColor;

struct Material {
    int textureId;
    float roughness;
    float metallic;
    float pad0;
    float pad1;
};

layout(set = 0, binding = 3) readonly buffer MaterialBuffer {
    Material materials[];
} materialBuffer;

layout(set = 0, binding = 4) uniform sampler2D textures[];

void main() {
    // Clip to scroll region (discard fragments outside clipRect)
    vec2 fragPos = gl_FragCoord.xy;
    if (fragPos.x < inClipRect.x || fragPos.y < inClipRect.y ||
        fragPos.x > inClipRect.z || fragPos.y > inClipRect.w) {
        discard;
    }

    Material mat = materialBuffer.materials[inMaterialId];
    float alpha = texture(textures[nonuniformEXT(mat.textureId)], inTexCoord).r;

    if (alpha < 0.01) {
        discard;
    }

    outColor = vec4(1.0, 1.0, 1.0, alpha);
}
