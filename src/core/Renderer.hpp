#pragma once

#include "Global.hpp"
#include "Command.hpp"
#include "Buffer.hpp"
#include "Image.hpp"
#include "Vertex.hpp"
#include "Pipeline.hpp"
#include "MeshPipeline.hpp"
#include "SkeletonMeshPipeline.hpp"
#include "ParticleSystemPipeline.hpp"
#include "ParticleSystemCompPipeline.hpp"
#include "ParticleCullPipeline.hpp"
#include "UberCullPipeline.hpp"
#include "DepthPipeline.hpp"
#include "SkeletonDepthPipeline.hpp"
#include "SkeletonCullPipeline.hpp"
#include "LightCullPipeline.hpp"
#include <mutex>

namespace Lca {
    namespace Core {
        const inline uint32_t MAX_OBJECTS =  (1024 * 256);
        const inline uint32_t MAX_MODEL_MATRICES =  MAX_OBJECTS;
        const inline uint32_t MAX_MATERIALS =  1024;
        const inline uint32_t MAX_MESHES =  1024;
        const inline uint32_t MAX_VERTICES =  1048576;
        const inline uint32_t MAX_INDICES =  1048576;   
        const inline uint32_t MAX_SHADERS = 1024;
        const inline uint32_t MAX_TEXTURES = 1024;
        const inline uint32_t MAX_LIGHTS = 1024;
        const inline uint32_t TILE_WIDTH = 16;
        const inline uint32_t TILE_HEIGHT = 16;
        const inline uint32_t MAX_LIGHTS_PER_TILE = 256;
        const inline uint32_t MAX_PARTICLE_SYSTEMS            = 1024;
        const inline uint32_t MAX_PARTICLES                  = 524288; // 512K particles
        const inline uint32_t MAX_PARTICLE_COMP_PIPELINES    = 64;
        // Each comp pipeline has its own strided region in the dispatch table.
        // Worst case: all MAX_PARTICLES assigned to a single pipeline.
        // sim workgroup size = 256 → max groups per pipeline = 524288 / 256 = 2048.
        const inline uint32_t MAX_DISPATCH_PER_PIPELINE      = MAX_PARTICLES / 256u; // 2048
        // Total dispatch table size = one 2048-entry region per comp pipeline.
        const inline uint32_t MAX_PARTICLE_DISPATCH_ENTRIES  = MAX_PARTICLE_COMP_PIPELINES * MAX_DISPATCH_PER_PIPELINE; // 131072
        // Maximum number of distinct particle graphics pipelines.
        // Each pipeline gets its own per-slot indirect buffer written by the cull shader.
        const inline uint32_t MAX_PARTICLE_GFX_PIPELINES     = 64;

        struct Camera{
            glm::vec4 frustumPlanes[6];
            glm::mat4 projection;
            glm::mat4 view;
            glm::mat4 viewProjection;
            glm::mat4 inverseProjection;
            glm::vec4 camPos;        // xyz = world position, w = unused (matches std140 vec3 padded to vec4)
            glm::vec4 camDir;        // xyz = direction, w = unused
            glm::vec2 screenSize;    // Width, height in pixels
            float nearClip;          // Near plane distance
            float farClip;           // Far plane distance
        };

        // Type constants for Light::position.w
        constexpr float LIGHT_TYPE_POINT       = 0.0f;
        constexpr float LIGHT_TYPE_SPOT        = 1.0f;
        constexpr float LIGHT_TYPE_DIRECTIONAL = 2.0f;

        struct Light{
            glm::vec4 position;  // xyz = world position, w = type (0=point, 1=spot, 2=directional)
            glm::vec4 direction; // xyz = direction (spot/directional), w = unused
            glm::vec4 color;     // rgb = color, a = intensity
            glm::vec4 params;    // x = radius, y = cos(innerCutoff), z = cos(outerCutoff), w = unused
        };

        struct ModelMatrix{
            glm::mat4 model{glm::mat4(1.0f)};
            glm::mat4 normal{glm::mat4(1.0f)};
            glm::vec4 scale{glm::vec4(1.0f)};
            glm::vec4 rotation{glm::vec4(0.0f, 0.0f, 0.0f, 1.0f)};
            glm::vec4 position{glm::vec4(0.0f)};
        };

        struct ObjectInstance {
            uint32_t transformID{UINT32_MAX}; // Index into a ModelMatrix buffer
            uint32_t shaderID{UINT32_MAX};       // Index of the IndirectBuffer to write to
            uint32_t meshID{UINT32_MAX};         // ID to look up mesh details (indexCount, etc)
            uint32_t materialID{UINT32_MAX};
        };

        struct SkeletonMeshInstance {
            uint32_t transformID{UINT32_MAX};
            uint32_t shaderID{UINT32_MAX};
            uint32_t meshID{UINT32_MAX};
            uint32_t materialID{UINT32_MAX};
            uint32_t skeletonInstanceID{UINT32_MAX};
            int32_t nodeIndex{-1}; // -1 for deformed, >= 0 for node-attached
        };

        static constexpr uint32_t MAX_SKELETON_INSTANCES = 1024;
        static constexpr uint32_t MAX_BONES_PER_SKELETON = 256;

        struct SkeletonInstance {
            glm::mat4 boneWithInvBindMatrices[MAX_BONES_PER_SKELETON];
            glm::mat4 boneTransform[MAX_BONES_PER_SKELETON];
        };

        // ── Particle system data structures ───────────────────────────────────

        // Per-particle state in the GPU storage buffer.
        // position.xyz = system-local position, position.w = remaining lifetime (seconds).
        // velocity.w   = max lifetime (seconds, already tracked in the struct).
        // params.x     = particle size scale.
        // params.y     = systemIndex stored as uint bits (uintBitsToFloat / floatBitsToUint).
        struct Particle {
            glm::vec4 position;  // xyz = local pos, w = remaining lifetime
            glm::vec4 velocity;  // xyz = velocity,  w = max lifetime
            glm::vec4 color;     // rgba tint
            glm::vec4 params;    // x = size, y = systemIndex bits, zw = user params
        };

        // Per-particle-system GPU record, synced via DualBuffer (CPU add/remove/update).
        // The `active` field is additionally written every frame by the ParticleCullPipeline.
        // `indexCount`, `firstIndex`, `vertexOffset` are filled CPU-side at addParticleSystem
        // time so the cull shader can build VkDrawIndexedIndirectCommand[slot] without a
        // separate mesh-info SSBO binding.
        struct ParticleSystemInstance {
            // ── Identifiers ──────────────────────────────────────────
            uint32_t  transformID;         // parent ModelMatrix index
            uint32_t  computeID;           // ParticleSystemCompPipeline variant
            uint32_t  graphicsID;          // ParticleSystemPipeline variant
            uint32_t  meshID;              // shape mesh
            uint32_t  materialID;
            uint32_t  particleOffset;      // first particle in storage buffer
            uint32_t  particleCount;
            uint32_t  active;              // 0 = culled, 1 = visible (written by cull shader)
            // ── Draw command data (CPU-written, read by cull shader) ─
            uint32_t  indexCount;
            uint32_t  firstIndex;
            int32_t   vertexOffset;
            float     boundingSphereRadius;// world-space sphere radius for frustum culling
            // ── Local particle-system offset from parent transform ───
            glm::mat4 localOffset;
        };

        // Per-workgroup record written by the cull shader into particleDispatchTableBuffer.
        // The sim shader reads dispatchTable[gl_WorkGroupID.x] to find its system
        // and particle range — so no push constant is needed per dispatch.
        struct ParticleDispatchEntry {
            uint32_t particleBase; // = particleOffset + workgroupLocalIdx * 256 (absolute)
            uint32_t systemSlot;  // index into ParticleSystemInstances[]
            uint32_t _pad[2];
        };

        // deltaTime-only uniform for the particle simulation compute shader.
        // Remaining lifetime and max lifetime are stored per-particle in Particle.position.w
        // and Particle.velocity.w respectively, so no totalTime is needed here.
        struct ParticleDeltaTimeUniform {
            float deltaTime;
            float _pad[3];
        };

        class Renderer{
        public:
            void init();
            void shutdown();

            void recordFrame(uint32_t frameIndex);
            void submitFrame(uint32_t frameIndex);
            uint32_t addModelMatrix(const ModelMatrix& matrix);
            inline void updateModelMatrix(uint32_t id, const ModelMatrix& matrix){
                modelMatrices.update(id, matrix);
            }   
            void removeModelMatrix(uint32_t id);
            void copyModelMatricesToGPU(uint32_t frameIndex){
                // Returns only the slots that changed; static entities become
                // clean after MAX_FRAMES_IN_FLIGHT uploads and produce no ranges.
                dirtyModelMatrixIndices[frameIndex] = modelMatrices.copyTo(modelMatricesGPU[frameIndex].interface);
            }
            uint32_t addObjectInstance(const ObjectInstance& instance);
            void updateObjectInstance(uint32_t id, const ObjectInstance& instance);
            void removeObjectInstance(uint32_t id);
            void copyObjectInstancesToGPU(uint32_t frameIndex){
                dirtyObjectInstanceIndices[frameIndex] = objectInstances.copyTo(objectInstancesGPU[frameIndex].interface);
            }
            uint32_t addMeshPipeline(const std::string& name, MeshPipeline&& pipeline, uint32_t maxObjects = MAX_OBJECTS);
            uint32_t getMeshPipelineId(const std::string& name) const;
                std::array<Buffer, MAX_FRAMES_IN_FLIGHT> depthPrePassBuffer;
                std::array<Buffer, MAX_FRAMES_IN_FLIGHT> depthPrePassCountBuffer;

            void updatePipelineDescriptorSets();

            // Light management (packed, rebuilt every frame)
            uint32_t addLight(const Light& light);
            void updateLight(uint32_t id, const Light& light);
            void removeLight(uint32_t id);
            void copyLightsToGPU(uint32_t frameIndex);
            uint32_t getLightCount() const { return packedLightCount; }
            const Buffer getLightBuffer(uint32_t frameIndex) const { return lightsGPU[frameIndex].buffer; }

            const Buffer getObjectInstanceBuffer(uint32_t frameIndex) const { return objectInstancesGPU[frameIndex].buffer; }
            const Buffer getModelMatrixBuffer(uint32_t frameIndex) const { return modelMatricesGPU[frameIndex].buffer; }
            const Buffer getDrawCountBuffer(uint32_t frameIndex) const { return drawCounts[frameIndex]; }
                const Buffer getDepthPrePassBuffer(uint32_t frameIndex) const { return depthPrePassBuffer[frameIndex]; }
                const Buffer getDepthPrePassCountBuffer(uint32_t frameIndex) const { return depthPrePassCountBuffer[frameIndex]; }
            const Buffer getShaderCapacityBuffer() const { return shaderCapacities.buffer; }
            const Buffer getIndirectBuffer(uint32_t frameIndex, uint32_t index) const { return indirectBuffers[frameIndex][index]; }
            const std::vector<Buffer>& getIndirectBuffers(uint32_t frameIndex) const { return indirectBuffers[frameIndex]; }
            VkBuffer getDummyIndirectVkBuffer() const { return dummyIndirectBuffer.vkBuffer; }
            uint32_t getMaxShaderCount() const { return MAX_SHADERS; }
            
            void updateCameraBuffer(uint32_t frameIndex, const Camera& camera){
                memcpy(cameraBuffer[frameIndex].interface.pMemory, &camera, sizeof(Camera));
            }
            const Buffer getCameraBuffer(uint32_t frameIndex) const { return cameraBuffer[frameIndex].buffer; }
            void updateCamera(uint32_t frameIndex, glm::vec3 position, glm::vec3 direction, float fov, float nearClip, float farClip);
            Texture& getDepthMap(uint32_t frameIndex);
            const Buffer getLightIndicesBuffer(uint32_t frameIndex) const { return lightIndicesBuffer[frameIndex]; }

            // ── Skeleton mesh instances ────────────────────────────
            uint32_t addSkeletonMeshInstance(const SkeletonMeshInstance& instance);
            void updateSkeletonMeshInstance(uint32_t id, const SkeletonMeshInstance& instance);
            void removeSkeletonMeshInstance(uint32_t id);
            void copySkeletonMeshInstancesToGPU(uint32_t frameIndex){
                dirtySkeletonMeshInstanceIndices[frameIndex] = skeletonMeshInstances.copyTo(skeletonMeshInstancesGPU[frameIndex].interface);
            }
            const Buffer getSkeletonMeshInstanceBuffer(uint32_t frameIndex) const { return skeletonMeshInstancesGPU[frameIndex].buffer; }

            // ── Skeleton instances (bone matrices) ─────────────────
            uint32_t addSkeletonInstance();
            void updateSkeletonInstance(uint32_t frameIndex, uint32_t id, const SkeletonInstance& instance);
            void removeSkeletonInstance(uint32_t id);
            const Buffer getSkeletonInstanceBuffer(uint32_t frameIndex) const { return skeletonInstancesGPU[frameIndex].buffer; }

            // ── Skeleton mesh pipelines ────────────────────────────
            uint32_t addSkeletonMeshPipeline(const std::string& name, SkeletonMeshPipeline&& pipeline, uint32_t maxObjects = MAX_OBJECTS);
            uint32_t getSkeletonMeshPipelineId(const std::string& name) const;
            const std::vector<Buffer>& getSkeletonIndirectBuffers(uint32_t frameIndex) const { return skeletonIndirectBuffers[frameIndex]; }
            const Buffer getSkeletonDrawCountBuffer(uint32_t frameIndex) const { return skeletonDrawCounts[frameIndex]; }
            const Buffer getSkeletonShaderCapacityBuffer() const { return skeletonShaderCapacities.buffer; }
            VkBuffer getSkeletonDummyIndirectVkBuffer() const { return skeletonDummyIndirectBuffer.vkBuffer; }

            // Skeleton depth pre-pass buffers
            std::array<Buffer, MAX_FRAMES_IN_FLIGHT> skeletonDepthPrePassBuffer;
            std::array<Buffer, MAX_FRAMES_IN_FLIGHT> skeletonDepthPrePassCountBuffer;

            // ── Particle system management ─────────────────────────
            // addParticleSystem allocates a contiguous particle range in the
            // GPU storage buffer (same first-fit strategy as AssetManager meshes),
            // writes one VkDrawIndexedIndirectCommand per system into the graphics
            // pipeline's indirect buffer, and returns the slot ID used to update or
            // remove the system later.
            uint32_t addParticleSystem(const ParticleSystemInstance& instance);
            void     updateParticleSystem(uint32_t id, const ParticleSystemInstance& instance);
            void     removeParticleSystem(uint32_t id);
            void     copyParticleSystemInstancesToGPU(uint32_t frameIndex) {
                dirtyParticleSystemIndices[frameIndex] = particleSystemSlots.copyTo(particleSystemInstancesGPU[frameIndex].interface);
            }
            // Updates the deltaTime uniform that the sim shader reads each frame.
            // Call once per frame before recordFrame (typically from the ECS particle system).
            void     updateParticleDeltaTime(uint32_t frameIndex, float deltaTime);

            // ── Particle compute pipelines ─────────────────────────
            uint32_t addParticleSystemCompPipeline(const std::string& name, ParticleSystemCompPipeline&& pipeline);
            uint32_t getParticleSystemCompId(const std::string& name) const;

            // ── Particle graphics pipelines ────────────────────────
            uint32_t addParticleSystemPipeline(const std::string& name, ParticleSystemPipeline&& pipeline);
            uint32_t getParticleSystemPipelineId(const std::string& name) const;

            // ── Particle buffer accessors (used by pipeline constructors) ──
            const Buffer getParticleSystemInstanceBuffer(uint32_t frameIndex) const { return particleSystemInstancesGPU[frameIndex].buffer; }
            const Buffer getParticleStorageBuffer()          const { return particleStorageBuffer; }
            const Buffer getParticleDeltaTimeBuffer(uint32_t frameIndex) const { return particleDeltaTimeBuffer[frameIndex].buffer; }
            // Per-pipeline indirect buffers written every frame by the particle cull pipeline.
            // particleGfxIndirectBuffers[graphicsID][slot] = VkDrawIndexedIndirectCommand;
            //   firstInstance = particleOffset, instanceCount = particleCount (0 = culled).
            const std::vector<Buffer>& getParticleGfxIndirectBuffers() const { return particleGfxIndirectBuffers; }
            const Buffer getParticleGfxDrawCountBuffer(uint32_t frameIndex) const { return particleGfxDrawCounts[frameIndex]; }
            // particleCompIndirectBuffer[K] = VkDispatchIndirectCommand for comp pipeline K.
            // Reset to {0,1,1} each frame by CPU (vkCmdUpdateBuffer); cull atomically
            // accumulates workgroup counts into .x.  One vkCmdDispatchIndirect per pipeline.
            const Buffer getParticleCompIndirectBuffer()   const { return particleCompIndirectBuffer; }
            // particleDispatchTableBuffer[entry] = { particleBase, systemSlot }.
            // Written by cull shader; read by sim shader via gl_WorkGroupID.x.
            const Buffer getParticleDispatchTableBuffer()  const { return particleDispatchTableBuffer; }
        
        private:
            
            SlotBuffer<ObjectInstance, MAX_OBJECTS> objectInstances;
            std::array<DualBuffer, MAX_FRAMES_IN_FLIGHT> objectInstancesGPU;
            // Dirty slot indices written to the staging buffer this frame.
            // Static entities (Component::Static) stop appearing here after
            // their initial MAX_FRAMES_IN_FLIGHT uploads.
            std::array<std::vector<uint32_t>, MAX_FRAMES_IN_FLIGHT> dirtyObjectInstanceIndices;

            SlotBuffer<ModelMatrix, MAX_MODEL_MATRICES> modelMatrices;
            std::array<DualBuffer, MAX_FRAMES_IN_FLIGHT> modelMatricesGPU;
            std::array<std::vector<uint32_t>, MAX_FRAMES_IN_FLIGHT> dirtyModelMatrixIndices;
            
            // Lights use a packed (dense) array, not SlotBuffer.
            // light_cull.comp iterates lightCount entries assuming no holes.
            // With MAX_LIGHTS=1024, rebuilding the packed buffer each frame
            // is ~64KB — trivially cheap vs. wasting compute threads on holes.
            std::array<Light, MAX_LIGHTS> lightSlots{};
            std::array<bool, MAX_LIGHTS> lightSlotActive{};
            uint32_t lightCount{0};
            std::vector<uint32_t> freeLightSlots;
            bool lightsDirty{false};
            uint32_t packedLightCount{0};
            std::array<DualBuffer, MAX_FRAMES_IN_FLIGHT> lightsGPU;
            std::array<Buffer, MAX_FRAMES_IN_FLIGHT> lightIndicesBuffer;
            std::array<Texture, MAX_FRAMES_IN_FLIGHT> depthMaps;
            
            std::vector<MeshPipeline> meshPipelines;
            std::unordered_map<std::string, uint32_t> meshPipelineMap;
            std::array<std::vector<Buffer>, MAX_FRAMES_IN_FLIGHT> indirectBuffers;
            std::unique_ptr<UberCullPipeline> uberCullPipeline;
            std::unique_ptr<DepthPipeline> depthPipeline;
            std::unique_ptr<LightCullPipeline> lightCullPipeline;
            Buffer dummyIndirectBuffer;
            std::array<Buffer, MAX_FRAMES_IN_FLIGHT> drawCounts;
            DualBuffer shaderCapacities;
            std::array<DualBuffer, MAX_FRAMES_IN_FLIGHT> cameraBuffer;

            // ── Skeleton mesh instance data ────────────────────────
            SlotBuffer<SkeletonMeshInstance, MAX_OBJECTS> skeletonMeshInstances;
            std::array<DualBuffer, MAX_FRAMES_IN_FLIGHT> skeletonMeshInstancesGPU;
            std::array<std::vector<uint32_t>, MAX_FRAMES_IN_FLIGHT> dirtySkeletonMeshInstanceIndices;

            // ── Skeleton instance data (bone matrices) ─────────────
            std::array<DualBuffer, MAX_FRAMES_IN_FLIGHT> skeletonInstancesGPU;
            std::array<std::vector<uint32_t>, MAX_FRAMES_IN_FLIGHT> dirtySkeletonInstanceIndices_;
            std::array<std::mutex, MAX_FRAMES_IN_FLIGHT> skeletonInstanceMutexes;
            std::vector<uint32_t> freeSkeletonInstanceSlots;
            uint32_t skeletonInstanceCount{0};

            // ── Skeleton mesh pipeline data ────────────────────────
            std::vector<SkeletonMeshPipeline> skeletonMeshPipelines;
            std::unordered_map<std::string, uint32_t> skeletonMeshPipelineMap;
            std::array<std::vector<Buffer>, MAX_FRAMES_IN_FLIGHT> skeletonIndirectBuffers;
            Buffer skeletonDummyIndirectBuffer;
            std::array<Buffer, MAX_FRAMES_IN_FLIGHT> skeletonDrawCounts;
            DualBuffer skeletonShaderCapacities;
            std::unique_ptr<SkeletonDepthPipeline> skeletonDepthPipeline;
            std::unique_ptr<SkeletonCullPipeline> skeletonCullPipeline;

            // ── Particle storage buffer ────────────────────────────
            // One large device-local buffer shared by all particle systems.
            // Ranges are allocated with a first-fit free-list, identical to
            // the vertex/index buffer strategy in AssetManager.
            struct ParticleBufferRange { uint32_t offset; uint32_t size; };
            Buffer particleStorageBuffer;
            uint32_t currentParticleTop{0};
            std::vector<ParticleBufferRange> particleFreeRanges;
            uint32_t allocateParticleRange(uint32_t count);
            void     freeParticleRange(uint32_t offset, uint32_t count);

            // ── Particle system instance data ──────────────────────
            SlotBuffer<ParticleSystemInstance, MAX_PARTICLE_SYSTEMS> particleSystemSlots;
            std::array<DualBuffer, MAX_FRAMES_IN_FLIGHT> particleSystemInstancesGPU;
            std::array<std::vector<uint32_t>, MAX_FRAMES_IN_FLIGHT> dirtyParticleSystemIndices;

            // ── Per-frame deltaTime uniform ────────────────────────
            std::array<DualBuffer, MAX_FRAMES_IN_FLIGHT> particleDeltaTimeBuffer;

            // particleGfxIndirectBuffers[graphicsID] = Buffer of MAX_PARTICLE_SYSTEMS
            //   VkDrawIndexedIndirectCommands indexed by global particle system slot.
            //   firstInstance = particleOffset so gl_InstanceIndex = absolute particle index.
            //   Cull shader writes into the matching pipeline's buffer; all others stay 0
            //   because each buffer is zeroed via vkCmdFillBuffer before the cull dispatch.
            std::vector<Buffer> particleGfxIndirectBuffers;
            // Per-frame draw count buffer: one uint32_t per registered gfx pipeline.
            // Cleared to 0 each frame; cull shader atomically increments per visible system.
            std::array<Buffer, MAX_FRAMES_IN_FLIGHT> particleGfxDrawCounts;
            // particleCompIndirectBuffer[K] = VkDispatchIndirectCommand for comp pipeline K.
            //   CPU resets to {0,1,1} each frame via vkCmdUpdateBuffer; cull shader
            //   atomically accumulates x = total workgroup count for visible systems.
            //   One vkCmdDispatchIndirect per pipeline reads this.
            Buffer particleCompIndirectBuffer;
            // particleDispatchTableBuffer[entry] = ParticleDispatchEntry.
            //   Written by cull shader; maps global workgroup ID → (particleBase, systemSlot).
            //   Allows the sim shader to find its system without a push constant.
            Buffer particleDispatchTableBuffer;

            // ── Particle graphics pipeline data ─────────────────────
            std::vector<ParticleSystemPipeline> particlePipelines;
            std::unordered_map<std::string, uint32_t> particlePipelineMap;
            // Per gfx pipeline: ordered list of system slots (for draw calls)
            std::vector<std::vector<uint32_t>> particleGfxSystems;

            // ── Particle compute pipeline data ──────────────────────
            std::vector<std::unique_ptr<ParticleSystemCompPipeline>> particleCompPipelines;
            std::unordered_map<std::string, uint32_t> particleCompPipelineMap;
            // Per comp pipeline: list of registered system slots
            std::vector<std::vector<uint32_t>> particleCompSystems;

            // ── Particle cull pipeline ──────────────────────────────
            std::unique_ptr<ParticleCullPipeline> particleCullPipeline;

            // Total number of particle system slots ever allocated (high-water mark
            // into particleSystemInstances, used for cull dispatch sizing).
            uint32_t particleSystemRegisteredCount{0};

            
            glm::mat4 createViewMatrix(const glm::vec3& cameraPosition, const glm::vec3& cameraDirection, const glm::vec3& upDirection);
            glm::mat4 createProjectionMatrix(float fov, float aspectRatio, float nearClip, float farClip);
            glm::mat4 createXMatrix();
        
        };

        inline Renderer globalRenderer;
        inline Renderer& GetRenderer() {
            return globalRenderer;
        }
    }
}