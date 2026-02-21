#pragma once

#include "Global.hpp"
#include "Buffer.hpp"
#include "Image.hpp"
#include "Vertex.hpp"
#include "Renderer.hpp"
namespace Lca{
    namespace Core{
        
        struct Material{
            int32_t textureId;
            float roughness;
            float metallic;
            float padding[2];
        };


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

            const Buffer getMaterialBuffer() const { return materials.buffer; }
            const std::array<Texture, MAX_TEXTURES>& getTextures() const { return textures; }
            const Buffer getMeshInfoBuffer() const { return meshInfos.buffer; }
            const Buffer getVertexBuffer() const { return vertexBuffer; }
            const Buffer getIndexBuffer() const { return indexBuffer; }
            
        private:
            Buffer vertexBuffer;
            Buffer indexBuffer;
            
            SlotBufferGPU materials;
            SlotBufferGPU meshInfos;
            
            std::array<Texture, MAX_TEXTURES> textures;
            std::array<bool, MAX_TEXTURES> textureSlotOwned;
            std::vector<uint32_t> freeTextureSlots;

            struct MeshInfo{
                uint32_t indexCount;
                uint32_t firstIndex;
                uint32_t vertexCount;
                int32_t vertexOffset;
                glm::vec4 boundingSphere; // xyz = local center, w = radius
            };

            struct BufferRange {
                uint32_t offset;
                uint32_t size;
            };

            std::unordered_map<std::string, uint32_t> materialMap;
            std::unordered_map<std::string, uint32_t> textureMap;
            std::unordered_map<std::string, uint32_t> meshMap;


            uint32_t currentVertexOffset = 0;
            uint32_t currentIndexOffset = 0;

            std::vector<BufferRange> freeVertexRanges;
            std::vector<BufferRange> freeIndexRanges;
            
            uint32_t allocateRange(std::vector<BufferRange>& freeList, uint32_t& currentTop, uint32_t size);
            void freeRange(std::vector<BufferRange>& freeList, uint32_t offset, uint32_t size);
        };

        AssetManager& GetAssetManager();

    }
}