#pragma once

#include "Global.hpp"
#include "Command.hpp"
#include "Buffer.hpp"
#include "Image.hpp"
#include "Vertex.hpp"
#include "Pipeline.hpp"
#include "MeshPipeline.hpp"
#include "UberCullPipeline.hpp"

namespace Lca {
    namespace Core {
        inline constexpr uint32_t MAX_OBJECTS =  (1024 * 256);
        inline constexpr uint32_t MAX_MODEL_MATRICES =  MAX_OBJECTS;
        inline constexpr uint32_t MAX_MATERIALS =  1024;
        inline constexpr uint32_t MAX_MESHES =  1024;
        inline constexpr uint32_t MAX_VERTICES =  1048576;
        inline constexpr uint32_t MAX_INDICES =  1048576;
        inline constexpr uint32_t MAX_SHADERS = 1024;
        inline constexpr uint32_t MAX_TEXTURES = 1024;

        struct Camera{
            glm::vec4 frustumPlanes[6];
            glm::mat4 viewProjection;
            glm::vec3 camPos;
            glm::vec3 camDir;
        };

        struct ModelMatrix{
            glm::mat4 model;
            glm::mat4 normal;
            glm::vec4 scale; // xyz = scale, w = padding
            glm::vec4 rotation; // quaternion
            glm::vec4 position;
        };

        struct ObjectInstance {
            uint32_t transformID; // Index into a ModelMatrix buffer
            uint32_t shaderID;       // Index of the IndirectBuffer to write to
            uint32_t meshID;         // ID to look up mesh details (indexCount, etc)
            uint32_t materialID;
        };

        class Renderer{
        public:
            void init();
            void shutdown();

            void recordFrame();
            void submitFrame();
            uint32_t addModelMatrix(const ModelMatrix& matrix);
            inline void updateModelMatrix(uint32_t id, const ModelMatrix& matrix){
                ModelMatrix* dst = mappedModelMatrices + id;
                *dst = matrix;
            }   
            void removeModelMatrix(uint32_t id);

            void resetObjectInstances() {
                nextObjectInstanceID = 0;
            }

            inline void addObjectInstance(const ObjectInstance& instance){
                //LCA_ASSERT(nextObjectInstanceID < MAX_OBJECTS, "Renderer", "addObjectInstance", "Exceeded MAX_OBJECTS capacity.")
                ObjectInstance* dst = mappedObjectInstances + nextObjectInstanceID;
                *dst = instance;
                nextObjectInstanceID++;
            }

            inline ObjectInstance* reserveObjectInstances(uint32_t count){
                //LCA_ASSERT(nextObjectInstanceID + count <= MAX_OBJECTS, "Renderer", "reserveObjectInstances", "Exceeded MAX_OBJECTS capacity.")
                ObjectInstance* dst = mappedObjectInstances + nextObjectInstanceID;
                nextObjectInstanceID += count;
                return dst;
            }
            
            uint32_t addMeshPipeline(const std::string& name, MeshPipeline&& pipeline, uint32_t maxObjects = MAX_OBJECTS);

            uint32_t getMeshPipelineId(const std::string& name) const;

            void updatePipelineDescriptorSets();

            const Buffer getObjectInstanceBuffer() const { return objectInstances.buffer; }
            const Buffer getModelMatrixBuffer() const { return modelMatrices.buffer; }
            const Buffer getDrawCountBuffer() const { return drawCounts; }
            const BufferInterface getShaderCapacityBuffer() const { return shaderCapacities; }
            const Buffer getIndirectBuffer(uint32_t index) const { return indirectBuffers[index]; }
            const std::vector<Buffer>& getIndirectBuffers() const { return indirectBuffers; }
            VkBuffer getDummyIndirectVkBuffer() const { return dummyIndirectBuffer.vkBuffer; }
            uint32_t getMaxShaderCount() const { return MAX_SHADERS; }
            
            void updateCameraBuffer(const Camera& camera){
                uint8_t* dst = static_cast<uint8_t*>(cameraBuffer.interface.pMemory);
                memcpy(dst, &camera, sizeof(Camera));
            }

            const Buffer getCameraBuffer() const { return cameraBuffer.buffer; }
            void updateCamera(glm::vec3 position, glm::vec3 direction, float fov, float nearClip, float farClip);
          
            private:
            
            uint32_t nextObjectInstanceID = 0;
            DualBuffer objectInstances;
            ObjectInstance* mappedObjectInstances = nullptr;

            SlotBuffer modelMatrices;
            ModelMatrix* mappedModelMatrices = nullptr;
            
            std::vector<MeshPipeline> meshPipelines;
            std::unordered_map<std::string, uint32_t> meshPipelineMap;
            std::unique_ptr<UberCullPipeline> uberCullPipeline;
            std::vector<Buffer> indirectBuffers;
            Buffer dummyIndirectBuffer;
            Buffer drawCounts;
            BufferInterface shaderCapacities;
            DualBuffer cameraBuffer; 

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