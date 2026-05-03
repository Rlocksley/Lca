#version 460
#extension GL_EXT_nonuniform_qualifier : enable

layout(location = 0) in vec2 inUV;
layout(location = 1) flat in uint inInstanceIndex;

layout(location = 0) out vec4 outColor;

struct GuiRectModel {
    vec4 rect;          // x, y, width, height (pixels)
    vec4 params;        // x = borderWidth, y = cornerRadius, z = materialId (float bits), w = borderMaterialId (float bits)
    vec4 clipRect;      // minX, minY, maxX, maxY (screen pixels)
    uint baseTransformID;
    uint _pad[3];
};

struct Material {
    int textureId;
    float roughness;
    float metallic;
    float pad0;
    float pad1;
};

layout(set = 0, binding = 1) readonly buffer GuiRectModelBuffer {
    GuiRectModel models[];
} guiRectModels;

layout(set = 0, binding = 3) readonly buffer MaterialBuffer {
    Material materials[];
} materialBuffer;

layout(set = 0, binding = 4) uniform sampler2D textures[];

// Signed distance from a point to a rounded rectangle centered at origin
// b = half-extents, r = corner radius
float sdRoundedBox(vec2 p, vec2 b, float r) {
    vec2 q = abs(p) - b + vec2(r);
    return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - r;
}

void main() {
    GuiRectModel rect = guiRectModels.models[inInstanceIndex];

    // Clip to scroll region (discard fragments outside clipRect)
    vec2 fragPos = gl_FragCoord.xy;
    if (fragPos.x < rect.clipRect.x || fragPos.y < rect.clipRect.y ||
        fragPos.x > rect.clipRect.z || fragPos.y > rect.clipRect.w) {
        discard;
    }

    float borderWidth = rect.params.x;
    float cornerRadius = rect.params.y;
    int materialId = floatBitsToInt(rect.params.z);
    int borderMaterialId = floatBitsToInt(rect.params.w);

    vec2 size = rect.rect.zw;  // width, height in pixels
    vec2 halfSize = size * 0.5;

    // Map UV (0→1) to local pixel coords centered at origin
    vec2 p = (inUV - 0.5) * size;

    // Clamp corner radius to half of the smallest dimension
    float r = min(cornerRadius, min(halfSize.x, halfSize.y));

    // Compute SDF distance
    float dist = sdRoundedBox(p, halfSize, r);

    // Anti-aliased edge (1px feather)
    float outerAlpha = 1.0 - smoothstep(-1.0, 0.0, dist);

    if (outerAlpha < 0.001) {
        discard;
    }

    // Determine fill color from fill material
    vec4 fillColor = vec4(1.0); // white when no material

    // Sample texture if material is valid
    if (materialId >= 0) {
        Material mat = materialBuffer.materials[materialId];
        fillColor = texture(textures[nonuniformEXT(mat.textureId)], inUV);
    }

    vec4 finalColor = fillColor;

    if (borderWidth > 0.0 && borderMaterialId >= 0) {
        // Sample border material texture
        Material borderMat = materialBuffer.materials[borderMaterialId];
        vec4 borderColor = texture(textures[nonuniformEXT(borderMat.textureId)], inUV);
        // Inner edge of border
        float innerDist = dist + borderWidth;
        float borderFactor = smoothstep(-1.0, 0.0, innerDist);
        finalColor = mix(fillColor, borderColor, borderFactor);
    }

    finalColor.a *= outerAlpha;
    outColor = finalColor;
}
