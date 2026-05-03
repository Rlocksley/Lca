#version 460

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec4 inColor;
layout(location = 3) in vec2 inTexCoord;

layout(location = 0) out vec2 outUV;
layout(location = 1) flat out uint outInstanceIndex;

struct GuiRectModel {
    vec4 rect;          // x, y, width, height (pixels)
    vec4 params;        // x = borderWidth, y = cornerRadius, z = materialId (float bits), w = borderMaterialId (float bits)
    vec4 clipRect;      // minX, minY, maxX, maxY (screen pixels)
    uint baseTransformID;
    uint _pad[3];
};

struct ModelMatrix {
    mat4 model;
    mat4 normal;
    vec4 scale;
    vec4 rotation;
    vec4 position;
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

layout(set = 0, binding = 1) readonly buffer GuiRectModelBuffer {
    GuiRectModel models[];
} guiRectModels;

layout(set = 0, binding = 2) readonly buffer ModelMatrixBuffer {
    ModelMatrix transforms[];
} modelMatrices;

void main() {
    GuiRectModel rect = guiRectModels.models[gl_InstanceIndex];
    ModelMatrix parentTransform = modelMatrices.transforms[rect.baseTransformID];

    // Orthographic projection: top-left origin (0,0), Y increases downward, units in pixels
    float W = cam.screenSize.x;
    float H = cam.screenSize.y;
    mat4 orthoProj = mat4(
        vec4(2.0 / W,  0.0,       0.0, 0.0),
        vec4(0.0,      2.0 / H,   0.0, 0.0),
        vec4(0.0,      0.0,      -1.0, 0.0),
        vec4(-1.0,    -1.0,       0.0, 1.0)
    );

    // Parent transform provides screen position (pixels) and Z for depth ordering
    vec3 screenPos = parentTransform.position.xyz;

    // Unit quad inPosition is [0,1]x[0,1] — scale to rect size, offset to rect origin
    vec2 localPos = inPosition.xy * rect.rect.zw + rect.rect.xy;

    // Compute final screen-space vertex
    vec3 screenVertex = screenPos + vec3(localPos, 0.0);

    gl_Position = orthoProj * vec4(screenVertex, 1.0);
    // Z-index: higher position.z = more in front (smaller depth).
    // Maps z ∈ [0,1] → depth ∈ [1,0]. Depth buffer clears to 1.0.
    gl_Position.z = 1.0 - screenPos.z;

    // UV for fragment shader (0→1 across the quad for SDF calculations)
    outUV = inPosition.xy;
    outInstanceIndex = gl_InstanceIndex;
}
