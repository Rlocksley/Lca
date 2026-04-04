#pragma once

#include "Global.hpp"
#include "Command.hpp"
#include "Buffer.hpp"
#include "Image.hpp"
#include "Vertex.hpp"
#include "Pipeline.hpp"
#include "MeshPipeline.hpp"
#include "SkeletonMeshPipeline.hpp"
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