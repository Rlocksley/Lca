#version 460

// fireball.vert — camera-facing billboard vertex shader for fireball particles.
// Each particle is a Square quad rotated to face the camera, positioned on the
// surface of the fireball sphere.

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec4 inColor;
layout(location = 3) in vec2 inTexCoord;

layout(location = 0) out vec3 outWorldNormal;
layout(location = 1) out vec4 outColor;
layout(location = 2) out vec2 outTexCoord;
layout(location = 3) flat out uint outMaterialId;
layout(location = 4) out vec3 outWorldPos;

// ── Structs ──────────────────────────────────────────────────────────────────

struct ModelMatrix {
    mat4 model;
    mat4 normalMatrix;
    vec4 scale;
    vec4 rotation;
    vec4 position;
};

struct ParticleSystemInstance {
    uint  transformID;
    uint  computeID;
    uint  graphicsID;
    uint  meshID;
    uint  materialID;
    uint  particleOffset;
    uint  particleCount;
    uint  isActive;
    uint  indexCount;
    uint  firstIndex;
    int   vertexOffset;
    float boundingSphereRadius;
    mat4  localOffset;
};

struct Particle {
    vec4 position;
    vec4 velocity;
    vec4 color;
    vec4 params;
};

// ── Descriptor bindings ───────────────────────────────────────────────────────

layout(set = 0, binding = 0) uniform CameraBuffer {
    vec4  frustumPlanes[6];
    mat4  projection;
    mat4  view;
    mat4  viewProjection;
    mat4  inverseProjection;
    vec4  camPos;
    vec4  camDir;
    vec2  screenSize;
    float nearClip;
    float farClip;
} cam;

layout(set = 0, binding = 1) readonly buffer ParticleSystemInstances {
    ParticleSystemInstance instances[];
};

layout(set = 0, binding = 2) readonly buffer ModelMatrices {
    ModelMatrix transforms[];
};

layout(set = 0, binding = 7) readonly buffer ParticleStorage {
    Particle particles[];
};

// ── Main ──────────────────────────────────────────────────────────────────────
void main() {
    Particle p = particles[gl_InstanceIndex];

    uint systemSlot = floatBitsToUint(p.params.y);
    ParticleSystemInstance inst = instances[systemSlot];
    ModelMatrix mmx = transforms[inst.transformID];

    // Particle world-space centre position (emitter local → world).
    mat4 world = mmx.model * inst.localOffset;
    vec3 particleWorldCenter = (world * vec4(p.position.xyz, 1.0)).xyz;

    // Camera-facing billboard vectors
    vec3 camRight = vec3(cam.view[0][0], cam.view[1][0], cam.view[2][0]);
    vec3 camUp    = vec3(cam.view[0][1], cam.view[1][1], cam.view[2][1]);

    // Billboard: offset the quad vertex along camera right/up axes
    float size   = p.params.x;
    vec3 worldPos = particleWorldCenter
                  + camRight * inPosition.x * size
                  + camUp    * inPosition.y * size;

    gl_Position   = cam.viewProjection * vec4(worldPos, 1.0);
    outWorldPos   = worldPos;

    // Normal points toward the camera
    outWorldNormal = -normalize(cam.camDir.xyz);

    outTexCoord   = inTexCoord;
    outMaterialId = inst.materialID;

    // Particle runtime color tint from sim shader
    outColor = p.color * inColor;
}
