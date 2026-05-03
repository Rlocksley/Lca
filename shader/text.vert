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
    mat4 transform = modelMatrices.transforms[text.baseTransformID].model * text.localTransform;

    vec4 worldPos = transform * vec4(inPosition, 1.0);
    gl_Position = cam.viewProjection * worldPos;

    outTexCoord = inTexCoord;
    outMaterialId = text.materialId;
    outClipRect = text.clipRect;
}
