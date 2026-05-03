#version 460

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec4 inColor;
layout(location = 3) in vec2 inTexCoord;

layout(location = 0) out vec2 outTexCoord;
layout(location = 1) flat out uint outMaterialId;
layout(location = 2) flat out vec4 outClipRect;

struct TextModel {
    mat4 localTransform;
    vec4 clipRect;
    uint baseTransformID;
    uint materialId;
    uint _pad0;
    uint _pad1;
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

layout(set = 0, binding = 1) readonly buffer TextModelBuffer {
    TextModel models[];
} textModels;

layout(set = 0, binding = 2) readonly buffer ModelMatrixBuffer {
    ModelMatrix transforms[];
} modelMatrices;

void main() {
    TextModel text = textModels.models[gl_InstanceIndex];
    ModelMatrix parentTransform = modelMatrices.transforms[text.baseTransformID];

    // Orthographic projection: top-left origin (0,0), Y increases downward, units in pixels
    // Vulkan NDC: X [-1,+1] left-to-right, Y [-1,+1] top-to-bottom
    float W = cam.screenSize.x;
    float H = cam.screenSize.y;
    mat4 orthoProj = mat4(
        vec4(2.0 / W,  0.0,       0.0, 0.0),
        vec4(0.0,      2.0 / H,   0.0, 0.0),
        vec4(0.0,      0.0,      -1.0, 0.0),
        vec4(-1.0,    -1.0,       0.0, 1.0)
    );

    // Extract position (screen pixels) and uniform scale from parent transform
    vec3 screenPos = parentTransform.position.xyz;
    float scale = parentTransform.scale.x;

    // Per-letter local transform positions the glyph (Y-up glyph space)
    vec4 localPos = text.localTransform * vec4(inPosition, 1.0);

    // Convert to screen space: scale glyph, flip Y for Y-down, offset to screen position
    vec3 screenVertex = screenPos + vec3(localPos.x * scale, -localPos.y * scale, 0.0);

    gl_Position = orthoProj * vec4(screenVertex, 1.0);
    // Z-index: higher position.z = more in front (smaller depth).
    // Maps z ∈ [0,1] → depth ∈ [1,0]. Depth buffer clears to 1.0.
    gl_Position.z = 1.0 - screenPos.z;

    outTexCoord = inTexCoord;
    outMaterialId = text.materialId;
    outClipRect = text.clipRect;
}
