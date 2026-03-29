#version 460

// Depth-only vertex shader for skeleton meshes with bone skinning

layout(location = 0) in vec3 inPosition;
layout(location = 1) in ivec4 inBones;
layout(location = 2) in vec4 inWeights;
layout(location = 3) in int inNodeIndex;

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

struct SkeletonMeshInstance {
    uint transformID;
    uint shaderID;
    uint meshID;
    uint materialID;
    uint skeletonInstanceID;
    int  nodeIndex;
};

struct ModelMatrix {
    mat4 model;
    mat4 normal;
    vec4 scale;
    vec4 rotation;
    vec4 position;
};

const uint MAX_BONES_PER_SKELETON = 256;
const uint MATS_PER_SKELETON = MAX_BONES_PER_SKELETON * 2;

layout(set = 0, binding = 1) readonly buffer SkeletonMeshInstanceBuffer {
    SkeletonMeshInstance instances[];
} objects;

layout(set = 0, binding = 2) readonly buffer ModelMatrixBuffer {
    ModelMatrix transforms[];
} modelMatrices;

layout(set = 0, binding = 3) readonly buffer SkeletonInstanceBuffer {
    mat4 data[];
} skeletonInstances;

void main() {
    SkeletonMeshInstance obj = objects.instances[gl_InstanceIndex];
    ModelMatrix transform = modelMatrices.transforms[obj.transformID];
    uint base = obj.skeletonInstanceID * MATS_PER_SKELETON;

    // Skinning: bone-weighted or node-attached
    mat4 skinMatrix;
    if (inNodeIndex == -1) {
        skinMatrix = inWeights.x * skeletonInstances.data[base + inBones.x]
                   + inWeights.y * skeletonInstances.data[base + inBones.y]
                   + inWeights.z * skeletonInstances.data[base + inBones.z]
                   + inWeights.w * skeletonInstances.data[base + inBones.w];
    } else {
        skinMatrix = skeletonInstances.data[base + MAX_BONES_PER_SKELETON + inNodeIndex];
    }

    vec4 skinnedPos = skinMatrix * vec4(inPosition, 1.0);
    vec4 worldPos = transform.model * skinnedPos;
    gl_Position = cam.viewProjection * worldPos;
}
