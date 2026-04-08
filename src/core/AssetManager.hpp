#pragma once

#include "Global.hpp"
#include "Buffer.hpp"
#include "Image.hpp"
#include "Vertex.hpp"
#include "Renderer.hpp"
#include <mutex>

namespace Lca{
    namespace Core{
        
        struct Material{
            int32_t textureId;
            float roughness;
            float metallic;
            float padding[2];
        };

        struct MeshInstance {
            std::string meshName;
            std::string materialName;
            uint32_t meshId;
            uint32_t materialId;
            int32_t nodeIndex{-1}; // -1 for deformed meshes, >= 0 for node-attached
        };

        struct VkNode{
            glm::ivec4 isBone_parentIndex_boneIndex;
            glm::mat4 offset;
        };

        // --- Animation data (SoA layout for cache-friendly sampling) ---

        struct PositionKeyFrame {
            float time;
            glm::vec3 value;
        };

        struct RotationKeyFrame {
            float time;
            glm::quat value;
        };

        struct ScaleKeyFrame {
            float time;
            glm::vec3 value;
        };

        struct AnimationChannel {
            std::string nodeName;
            int32_t nodeIndex = -1; // resolved by bindAnimationToSkeleton
            uint32_t positionOffset;
            uint32_t positionCount;
            uint32_t rotationOffset;
            uint32_t rotationCount;
            uint32_t scaleOffset;
            uint32_t scaleCount;
        };

        struct Animation {
            std::string name;
            float duration;
            float ticksPerSecond;
            std::vector<AnimationChannel> channels;
            // SoA keyframe storage — all channels' keyframes packed contiguously
            std::vector<PositionKeyFrame> positionKeys;
            std::vector<RotationKeyFrame> rotationKeys;
            std::vector<ScaleKeyFrame> scaleKeys;
        };

        struct NodePose {
            glm::vec3 position{0.0f};
            glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
            glm::vec3 scale{1.0f};
        };

        struct SkeletonNodeInfo {
            int32_t parentIndex{-1};
            bool isBone{false};
            glm::mat4 offsetMatrix{1.0f}; // inverse bind pose for bones, local transform for non-bones
        };

        struct SkeletonData {
            uint32_t skeletonId;
            uint32_t nodeCount{0};
            std::unordered_map<std::string, int32_t> nodeNameToIndex;
            std::vector<SkeletonNodeInfo> nodes;
        };

        using Model = std::vector<MeshInstance>;


        class AssetManager{
        public:
            void init();
            void shutdown();
            void recordSync(const Command& command);

            void loadTexture(const std::string& name, const std::string& filePath);
            void addTexture(const std::string& name, const Texture& texture);
            void removeTexture(const std::string& name);
            uint32_t getTextureId(const std::string& name) const;
            VkDescriptorImageInfo getTextureDescriptorInfo(uint32_t id) const;
            uint32_t getTextureSlotCount() const { return static_cast<uint32_t>(textures.size()); }

            uint32_t addMaterial(const std::string& name, const Material& material);
            void removeMaterial(const std::string& name);
            uint32_t getMaterialId(const std::string& name) const;

            uint32_t addMesh(const std::string& name, const std::vector<Vertex::Mesh>& vertices, const std::vector<uint32_t>& indices);
            void removeMesh(const std::string& name);
            uint32_t getMeshId(const std::string& name) const;

            uint32_t addSkeletonMesh(const std::string& name, const std::vector<Vertex::Skeleton>& vertices, const std::vector<uint32_t>& indices);
            void removeSkeletonMesh(const std::string& name);
            uint32_t loadSkeleton(const std::string& name, const std::string& filePath);
            Model loadSkeletonModel(const std::string& name, const std::string& filePath);

            std::vector<std::string> loadAnimations(const std::string& prefix, const std::string& filePath);
            const Animation& getAnimation(const std::string& name) const;
            void removeAnimation(const std::string& name);
            bool isAnimationCompatible(const std::string& skeletonName, const std::string& animationName) const;
            bool bindAnimationToSkeleton(const std::string& animationName, const std::string& skeletonName);
            std::string extractAnimationForSkeleton(const std::string& animationName, const std::string& skeletonName);
            const SkeletonData& getSkeletonData(const std::string& name) const;

            Model loadModel(const std::string& name, const std::string& filePath);
            const Model& getModel(const std::string& name) const;

            struct MeshInfo {
                uint32_t indexCount;
                uint32_t firstIndex;
                uint32_t vertexCount;
                int32_t  vertexOffset;
                glm::vec4 boundingSphere; // xyz = local center, w = radius
            };

            MeshInfo getMeshInfo(uint32_t meshId) const;

            const Buffer getMaterialBuffer() const { return materials.buffer; }
            const std::array<Texture, MAX_TEXTURES>& getTextures() const { return textures; }
            const Buffer getMeshInfoBuffer() const { return meshInfos.buffer; }
            const Buffer getSkeletonMeshInfoBuffer() const { return skeletonMeshInfos.buffer; }
            const Buffer getVertexBuffer() const { return vertexBuffer; }
            const Buffer getIndexBuffer() const { return indexBuffer; }
            const Buffer getSkeletonVertexBuffer() const { return skeletonVertexBuffer; }
            const Buffer getSkeletonIndexBuffer() const { return skeletonIndexBuffer; }
            const Buffer getSkeletonBuffer() const { return skeletonBuffer; }
            const Buffer getSkeletonNodeCountBuffer() const { return skeletonNodeCountBuffer; }
            
            
        private:
            Buffer vertexBuffer;
            Buffer indexBuffer;
            Buffer skeletonVertexBuffer;
            Buffer skeletonIndexBuffer;
            Buffer skeletonBuffer;
            Buffer skeletonNodeCountBuffer;
            
            SlotBufferGPU materials;
            SlotBufferGPU meshInfos;
            SlotBufferGPU skeletonMeshInfos;
            
            std::array<Texture, MAX_TEXTURES> textures;
            std::array<bool, MAX_TEXTURES> textureSlotOwned;
            std::vector<uint32_t> freeTextureSlots;

            static constexpr uint32_t MAX_SKELETONS = 1024;
            static constexpr uint32_t MAX_NODES_PER_SKELETON = 256;

            struct BufferRange {
                uint32_t offset;
                uint32_t size;
            };

            std::unordered_map<std::string, uint32_t> materialMap;
            std::unordered_map<std::string, uint32_t> textureMap;
            std::unordered_map<std::string, uint32_t> meshMap;
            std::unordered_map<std::string, uint32_t> skeletonMeshMap;
            std::unordered_map<std::string, uint32_t> skeletonMap;
            std::unordered_map<std::string, Model> modelMap;
            std::unordered_map<std::string, Model> skeletonModelMap;
            std::unordered_map<std::string, Animation> animationMap;
            std::unordered_map<std::string, SkeletonData> skeletonDataMap;
            std::vector<uint32_t> freeSkeletonSlots;


            uint32_t currentVertexOffset = 0;
            uint32_t currentIndexOffset = 0;

            uint32_t currentSkeletonVertexOffset = 0;
            uint32_t currentSkeletonIndexOffset = 0;

            std::vector<BufferRange> freeVertexRanges;
            std::vector<BufferRange> freeIndexRanges;

            std::vector<BufferRange> freeSkeletonVertexRanges;
            std::vector<BufferRange> freeSkeletonIndexRanges;
            
            uint32_t allocateRange(std::vector<BufferRange>& freeList, uint32_t& currentTop, uint32_t size);
            void freeRange(std::vector<BufferRange>& freeList, uint32_t offset, uint32_t size);

            mutable std::mutex assetMutex;  // guards meshMap, materialMap, free-lists, offsets, and command submission
        };

        AssetManager& GetAssetManager();

    }
}