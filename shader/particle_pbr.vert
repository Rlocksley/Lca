#version 460

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec4 inColor;
layout(location = 3) in vec2 inTexCoord;

layout(location = 0) out vec3 outWorldNormal;
layout(location = 1) out vec4 outColor;
layout(location = 2) out vec2 outTexCoord;
layout(location = 3) flat out uint outMaterialId;
layout(location = 4) out vec3 outWorldPos;

// ── Structs ──────────────────────────────────────────────────────────
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
    mat4  localOffset;  // local-to-parent transform of the emitter
};

// position.xyz = emitter-local particle position,  position.w = remaining lifetime
// velocity.xyz = velocity (m/s),                   velocity.w = max lifetime
// color        = rgba tint applied on top of the mesh vertex color
// params.x     = size scale
// params.y     = system slot stored as uint bits (floatBitsToUint / uintBitsToFloat)
struct Particle {
    vec4 position;
    vec4 velocity;
    vec4 color;
    vec4 params;
};

// ── Descriptor bindings ──────────────────────────────────────────────
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

// binding 7: particle storage buffer
// gl_InstanceIndex == firstInstance + local_instance == particleOffset + local_instance
// == absolute index of this particle in the storage buffer.
layout(set = 0, binding = 7) readonly buffer ParticleStorage {
    Particle particles[];
};

// ── Main ─────────────────────────────────────────────────────────────
void main() {
    // firstInstance is set to particleOffset by the cull shader, so
    // gl_InstanceIndex is the absolute index of this particle in the storage buffer.
    Particle p = particles[gl_InstanceIndex];

    // The simulation shader stores the owning system slot as uint bits in params.y.
    uint systemSlot = floatBitsToUint(p.params.y);
    ParticleSystemInstance inst = instances[systemSlot];
    ModelMatrix mmx = transforms[inst.transformID];

    // Scale the mesh vertex by the particle's size and translate it to the
    // particle's position in emitter-local space.
    vec3 localPos = inPosition * p.params.x + p.position.xyz;

    // Full world transform: parent model matrix * emitter-local offset matrix.
    mat4 world    = mmx.model * inst.localOffset;
    vec4 worldPos = world * vec4(localPos, 1.0);

    gl_Position = cam.viewProjection * worldPos;
    outWorldPos = worldPos.xyz;

    // Transform the mesh normal by the inverse-transpose of the world matrix.
    outWorldNormal = normalize(mat3(transpose(inverse(world))) * inNormal);

    // Blend mesh vertex color with the particle's runtime tint.
    outColor      = inColor * p.color;
    outTexCoord   = inTexCoord;
    outMaterialId = inst.materialID;
}
