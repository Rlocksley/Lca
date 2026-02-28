#pragma once

#include "Global.hpp"
#include "Command.hpp"
#include "Buffer.hpp"
#include "Image.hpp"
#include "Vertex.hpp"
#include "Pipeline.hpp"
#include "MeshPipeline.hpp"
#include "UberCullPipeline.hpp"
#include "DepthPipeline.hpp"
#include "LightCullPipeline.hpp"

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
                modelMatrices.copyTo(modelMatricesGPU[frameIndex].interface);
            }
            uint32_t addObjectInstance(const ObjectInstance& instance);
            void updateObjectInstance(uint32_t id, const ObjectInstance& instance);
            void removeObjectInstance(uint32_t id);
            void copyObjectInstancesToGPU(uint32_t frameIndex){
                objectInstances.copyTo(objectInstancesGPU[frameIndex].interface);
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
          
        private:
            
            SlotBuffer<ObjectInstance, MAX_OBJECTS> objectInstances;
            std::array<DualBuffer, MAX_FRAMES_IN_FLIGHT> objectInstancesGPU;
            
            SlotBuffer<ModelMatrix, MAX_MODEL_MATRICES> modelMatrices;
            std::array<DualBuffer, MAX_FRAMES_IN_FLIGHT> modelMatricesGPU;
            
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