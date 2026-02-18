#include "AssetManager.hpp"
#include "Queue.hpp"



namespace Lca{
    namespace Core{

        // Global AssetManager Instance
        static AssetManager globalAssetManager;

        AssetManager& GetAssetManager() {
            return globalAssetManager;
        }

        void AssetManager::recordSync(const Command& command) {
            meshInfos.recordSync(command);
            materials.recordSync(command);
        }

        void AssetManager::init(){
            vertexBuffer = createBuffer(MAX_VERTICES, sizeof(Vertex::Mesh), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
            indexBuffer = createBuffer(MAX_INDICES, sizeof(uint32_t), VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
            materials = createSlotBuffer(MAX_MATERIALS, sizeof(Material), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
            meshInfos = createSlotBuffer(MAX_MESHES, sizeof(MeshInfo), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

            currentVertexOffset = 0;
            currentIndexOffset = 0;
            freeVertexRanges.clear();
            freeIndexRanges.clear();
            materialMap.clear();
            meshMap.clear();

            uint32_t fallbackPixel = 0xFFFFFFFF;
            auto fallbackTexture = createTexture(1, 1, &fallbackPixel);

            for (uint32_t i = 0; i < MAX_TEXTURES; ++i) {
                textures[i] = fallbackTexture;
                textureSlotOwned[i] = false;
            }

            textureSlotOwned[0] = true;

            freeTextureSlots.clear();
            for (uint32_t i = MAX_TEXTURES - 1; i > 0; --i) {
                freeTextureSlots.push_back(i);
            }

            textureMap.clear();
        }

        void AssetManager::shutdown(){
            for (uint32_t id = 0; id < MAX_TEXTURES; ++id) {
                if (textureSlotOwned[id]) {
                    destroyTexture(textures[id]);
                }
            }
            
            textureMap.clear();
            freeTextureSlots.clear();
            materialMap.clear();
            meshMap.clear();
            freeVertexRanges.clear();
            freeIndexRanges.clear();

            destroySlotBuffer(meshInfos);
            destroySlotBuffer(materials);

            destroyBuffer(indexBuffer);
            destroyBuffer(vertexBuffer);
        }

        void AssetManager::loadTexture(const std::string& name, const std::string& filePath) {
            Core::TextureCPU textureCPU = Core::loadTexture(filePath);
            Core::Texture texture = Core::createTexture(textureCPU.width, textureCPU.height, textureCPU.pixels);
            Core::freeTexture(textureCPU);
            addTexture(name, texture);
        }

        void AssetManager::addTexture(const std::string& name, const Texture& texture){
            auto existing = textureMap.find(name);
            if (existing != textureMap.end()) {
                uint32_t id = existing->second;
                if (id < MAX_TEXTURES) {
                    if (textureSlotOwned[id]) {
                        destroyTexture(textures[id]);
                    }
                    textures[id] = texture;
                    textureSlotOwned[id] = true;
                }
                return;
            }

            if (freeTextureSlots.empty()) {
                LCA_LOGE("AssetManager", "addTexture", "No free texture slots available.");
            }

            uint32_t id = freeTextureSlots.back();
            freeTextureSlots.pop_back();
            textures[id] = texture;
            textureSlotOwned[id] = true;

            textureMap[name] = id;
        }

        void AssetManager::removeTexture(const std::string& name){
            auto it = textureMap.find(name);
            if (it == textureMap.end()) {
                LCA_LOGE("AssetManager", "removeTexture", "Texture with name " + name + " not found.");
            }

            uint32_t id = it->second;
            if (textureSlotOwned[id]) {
                destroyTexture(textures[id]);
            }

            textures[id] = textures[0];
            textureSlotOwned[id] = false;
            freeTextureSlots.push_back(id);
        
            textureMap.erase(it);
        }

        uint32_t AssetManager::getTextureId(const std::string& name) const {
            auto it = textureMap.find(name);
            if (it != textureMap.end()) {
                return it->second;
            }
            LCA_LOGE("AssetManager", "getTextureId", "Texture with name " + name + " not found.");
        }

        VkDescriptorImageInfo AssetManager::getTextureDescriptorInfo(uint32_t id) const {
            const Texture* selected = &textures[0];

            if (id < textures.size()) {
                selected = &textures[id];
            }

            VkDescriptorImageInfo info{};
            info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            info.imageView = selected->vkImageView;
            info.sampler = selected->vkSampler;
            return info;
        }

        uint32_t AssetManager::addMaterial(const std::string& name, const Material& material){
            if (materialMap.find(name) != materialMap.end()) {
                uint32_t id = materialMap[name];
                VkDeviceSize offset = id * materials.interface.elementSize;
                uint8_t* dst = static_cast<uint8_t*>(materials.interface.pMemory) + offset;
                memcpy(dst, &material, sizeof(Material));
                return id;
            }
            uint32_t id = materials.add((void*)&material);
            materialMap[name] = id;
            return id;
        }

        void AssetManager::removeMaterial(const std::string& name){
            if (materialMap.find(name) != materialMap.end()) {
                uint32_t id = materialMap[name];
                materials.remove(id);
                materialMap.erase(name);
                return;
            }
            LCA_LOGE("AssetManager", "removeMaterial", "Material with name " + name + " not found.");
        }

        uint32_t AssetManager::getMaterialId(const std::string& name) const {
            auto it = materialMap.find(name);
            if (it != materialMap.end()) {
                return it->second;
            }
            LCA_LOGE("AssetManager", "getMaterialId", "Material with name " + name + " not found.");
        }

        uint32_t AssetManager::allocateRange(std::vector<BufferRange>& freeList, uint32_t& currentTop, uint32_t size) {
            // First-fit strategy with efficient removal
            for (size_t i = 0; i < freeList.size(); ++i) {
                if (freeList[i].size >= size) {
                    uint32_t offset = freeList[i].offset;
                    
                    if (freeList[i].size > size) {
                        // Split the block
                        freeList[i].offset += size;
                        freeList[i].size -= size;
                    } else {
                        // Exact match, remove the block
                         freeList.erase(freeList.begin() + i);
                    }
                    return offset;
                }
            }
            
            // No suitable block found, allocate at the end
            uint32_t offset = currentTop;
            currentTop += size;
            return offset;
        }

        void AssetManager::freeRange(std::vector<BufferRange>& freeList, uint32_t offset, uint32_t size) {
            freeList.push_back({offset, size});
            // Optional: Coalesce free blocks here if desired to reduce fragmentation
        }

        uint32_t AssetManager::addMesh(const std::string& name, const std::vector<Vertex::Mesh>& vertices, const std::vector<uint32_t>& indices){
            LCA_ASSERT(meshMap.find(name) == meshMap.end(), "AssetManager", "addMesh",  "Mesh with name " + name + " already exists.");
            LCA_ASSERT(vertices.size() > 0, "AssetManager", "addMesh",  "Vertex list is empty for mesh " + name + ".");
            LCA_ASSERT(indices.size() > 0, "AssetManager", "addMesh",  "Index list is empty for mesh " + name + ".");

            uint32_t vertexCount = static_cast<uint32_t>(vertices.size());
            uint32_t indexCount = static_cast<uint32_t>(indices.size());

            uint32_t vertexOffsetVal = allocateRange(freeVertexRanges, currentVertexOffset, vertexCount);
            uint32_t indexOffsetVal = allocateRange(freeIndexRanges, currentIndexOffset, indexCount);

            VkDeviceSize vertexByteSize = vertexCount * sizeof(Vertex::Mesh);
            BufferInterface stagingVertex = createBufferInterface(vertexCount, sizeof(Vertex::Mesh), VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
            memcpy(stagingVertex.pMemory, vertices.data(), vertexByteSize);

            VkDeviceSize indexByteSize = indexCount * sizeof(uint32_t);
            BufferInterface stagingIndex = createBufferInterface(indexCount, sizeof(uint32_t), VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
            memcpy(stagingIndex.pMemory, indices.data(), indexByteSize);

            beginSingleCommand(singleCommand);

            VkBufferCopy copyRegion{};
            copyRegion.srcOffset = 0;
            copyRegion.dstOffset = vertexOffsetVal * sizeof(Vertex::Mesh);
            copyRegion.size = vertexByteSize;
            vkCmdCopyBuffer(singleCommand.vkCommandBuffer, stagingVertex.vkBuffer, vertexBuffer.vkBuffer, 1, &copyRegion);

            VkBufferCopy copyRegionIndex{};
            copyRegionIndex.srcOffset = 0;
            copyRegionIndex.dstOffset = indexOffsetVal * sizeof(uint32_t);
            copyRegionIndex.size = indexByteSize;
            vkCmdCopyBuffer(singleCommand.vkCommandBuffer, stagingIndex.vkBuffer, indexBuffer.vkBuffer, 1, &copyRegionIndex);

            endSingleCommand(singleCommand);
            submitSingleCommand(singleCommand);

            destroyBufferInterface(stagingIndex);
            destroyBufferInterface(stagingVertex);
        

            // 3. Register MeshInfo
            
            MeshInfo info{};
            info.indexCount = indexCount;
            info.firstIndex = indexOffsetVal;
            info.vertexCount = vertexCount;
            info.vertexOffset = static_cast<int32_t>(vertexOffsetVal);

            glm::vec3 minPos = vertices[0].position;
            glm::vec3 maxPos = vertices[0].position;
            for (const auto& v : vertices) {
                minPos = glm::min(minPos, v.position);
                maxPos = glm::max(maxPos, v.position);
            }
            glm::vec3 center = (minPos + maxPos) * 0.5f;
            float radius = 0.0f;
            for (const auto& v : vertices) {
                float dist = glm::distance(v.position, center);
                if (dist > radius) {
                    radius = dist;
                }
            }
            info.boundingSphere = glm::vec4(center, radius);
        
            uint32_t id = meshInfos.add(&info);
            // Internal tracking
            meshMap[name] = id;

            return id;
        }

        void AssetManager::removeMesh(const std::string& name){
            auto it = meshMap.find(name);
            if (it != meshMap.end()) {
                uint32_t id = it->second;
                
                MeshInfo info;
                meshInfos.remove(id, &info);

                freeRange(freeVertexRanges, static_cast<uint32_t>(info.vertexOffset), info.vertexCount);
                freeRange(freeIndexRanges, info.firstIndex, info.indexCount);

                meshMap.erase(name);
                return;
            }
            LCA_LOGE("AssetManager", "removeMesh", "Mesh with name " + name + " not found.");
        }

        uint32_t AssetManager::getMeshId(const std::string& name) const {
            auto it = meshMap.find(name);
            if (it != meshMap.end()) {
                return it->second;
            }
            LCA_LOGE("AssetManager", "getMeshId", "Mesh with name " + name + " not found.");
        }
    }
}