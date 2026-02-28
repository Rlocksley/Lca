#version 460

// Depth-only vertex shader for the DepthPipeline

layout(location = 0) in vec3 inPosition;

// Camera / projection data (binding matches Renderer usage)
layout(set = 0, binding = 0) uniform CameraBuffer {
    vec4 frustumPlanes[6];
    mat4 projection;       // Projection matrix
    mat4 view;             // View matrix (world → view)
    mat4 viewProjection;
    mat4 inverseProjection;// Inverse of projection
    vec3 camPos;
    vec3 camDir;
    vec2 screenSize;       // Width, height in pixels
    float nearClip;        // Near plane distance
    float farClip;         // Far plane distance
} cam;

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

layout(set = 0, binding = 1) readonly buffer ObjectBuffer {
    ObjectInstance instances[];
} objects;

layout(set = 0, binding = 2) readonly buffer ModelMatrixBuffer {
    ModelMatrix transforms[];
} modelMatrices;

void main() {
    // Instance ID comes from indirect/instanced draws
    ObjectInstance obj = objects.instances[gl_InstanceIndex];
    ModelMatrix transform = modelMatrices.transforms[obj.transformID];

    vec4 worldPos = transform.model * vec4(inPosition, 1.0);
    gl_Position = cam.viewProjection * worldPos;
}
