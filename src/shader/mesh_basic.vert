#version 460

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec4 inColor;
layout(location = 3) in vec2 inTexCoord;

layout(location = 0) out vec3 outWorldNormal;
layout(location = 1) out vec4 outColor;
layout(location = 2) out vec2 outTexCoord;
layout(location = 3) flat out uint outMaterialId;

struct CameraData {
    mat4 projView;
    vec4 position;
};

struct ModelMatrix {
    mat4 model;
    mat4 normal;
    vec4 scale;
    vec4 rotation;
    vec4 position;
};

struct ObjectInstance {
    uint transformID;
    uint shaderID;
    uint meshID;
    uint materialID;
};

layout(set = 0, binding = 0) uniform CameraBuffer {
    vec4 frustumPlanes[6];
    mat4 viewProjection;
    vec3 camPos;
    vec3 camDir;
}cam;

layout(set = 0, binding = 1) readonly buffer ObjectBuffer {
    ObjectInstance instances[];
} objects;

layout(set = 0, binding = 2) readonly buffer ModelMatrixBuffer {
    ModelMatrix transforms[];
} modelMatrices;

void main() {
    ObjectInstance obj = objects.instances[gl_InstanceIndex];
    ModelMatrix transform = modelMatrices.transforms[obj.transformID];

    vec4 worldPos = transform.model * vec4(inPosition, 1.0);
    gl_Position = cam.viewProjection * worldPos;

    outWorldNormal = normalize((transform.normal * vec4(inNormal, 0.0)).xyz);
    outColor = inColor;
    outTexCoord = inTexCoord;
    outMaterialId = obj.materialID;
}
