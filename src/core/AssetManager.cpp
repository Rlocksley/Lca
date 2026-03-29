#include "AssetManager.hpp"
#include "Queue.hpp"
#include "PhysicalDevice.hpp"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <filesystem>

#include "stb/stb_image.h"


namespace Lca{
    namespace Core{

        static glm::mat4 toGlmMat4(const aiMatrix4x4& m) {
            glm::mat4 out(1.0f);
            out[0][0] = m.a1; out[1][0] = m.a2; out[2][0] = m.a3; out[3][0] = m.a4;
            out[0][1] = m.b1; out[1][1] = m.b2; out[2][1] = m.b3; out[3][1] = m.b4;
            out[0][2] = m.c1; out[1][2] = m.c2; out[2][2] = m.c3; out[3][2] = m.c4;
            out[0][3] = m.d1; out[1][3] = m.d2; out[2][3] = m.d3; out[3][3] = m.d4;
            return out;
        }

        // Global AssetManager Instance
        static AssetManager globalAssetManager;

        AssetManager& GetAssetManager() {
            return globalAssetManager;
        }

        void AssetManager::recordSync(const Command& command) {
            meshInfos.recordSync(command);
            skeletonMeshInfos.recordSync(command);
            materials.recordSync(command);
        }

        void AssetManager::init(){
            vertexBuffer = createBuffer(MAX_VERTICES, sizeof(Vertex::Mesh), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
            indexBuffer = createBuffer(MAX_INDICES, sizeof(uint32_t), VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
            skeletonVertexBuffer = createBuffer(MAX_VERTICES, sizeof(Vertex::Skeleton), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
            skeletonIndexBuffer = createBuffer(MAX_INDICES, sizeof(uint32_t), VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
            skeletonBuffer = createBuffer(MAX_SKELETONS * MAX_NODES_PER_SKELETON, sizeof(VkNode), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
            skeletonNodeCountBuffer = createBuffer(MAX_SKELETONS, sizeof(uint32_t), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
            materials = createSlotBufferGPU(MAX_MATERIALS, sizeof(Material), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
            meshInfos = createSlotBufferGPU(MAX_MESHES, sizeof(MeshInfo), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
            skeletonMeshInfos = createSlotBufferGPU(MAX_MESHES, sizeof(MeshInfo), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

            currentVertexOffset = 0;
            currentIndexOffset = 0;
            currentSkeletonVertexOffset = 0;
            currentSkeletonIndexOffset = 0;
            freeVertexRanges.clear();
            freeIndexRanges.clear();
            freeSkeletonVertexRanges.clear();
            freeSkeletonIndexRanges.clear();
            materialMap.clear();
            meshMap.clear();
            skeletonMeshMap.clear();
            skeletonMap.clear();
            modelMap.clear();
            skeletonModelMap.clear();
            animationMap.clear();
            skeletonDataMap.clear();
            freeSkeletonSlots.clear();
            for (uint32_t i = MAX_SKELETONS; i > 0; --i) {
                freeSkeletonSlots.push_back(i - 1);
            }

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
            skeletonMeshMap.clear();
            skeletonMap.clear();
            modelMap.clear();
            skeletonModelMap.clear();
            animationMap.clear();
            skeletonDataMap.clear();            animationMap.clear();
            skeletonDataMap.clear();            freeSkeletonSlots.clear();
            freeVertexRanges.clear();
            freeIndexRanges.clear();
            freeSkeletonVertexRanges.clear();
            freeSkeletonIndexRanges.clear();

            destroySlotBufferGPU(skeletonMeshInfos);
            destroySlotBufferGPU(meshInfos);
            destroySlotBufferGPU(materials);

            destroyBuffer(skeletonIndexBuffer);
            destroyBuffer(skeletonVertexBuffer);
            destroyBuffer(skeletonBuffer);
            destroyBuffer(skeletonNodeCountBuffer);
            destroyBuffer(indexBuffer);
            destroyBuffer(vertexBuffer);

            
        }

        uint32_t AssetManager::loadSkeleton(const std::string& name, const std::string& filePath) {
            {
                std::lock_guard<std::mutex> lock(assetMutex);
                auto existing = skeletonMap.find(name);
                if (existing != skeletonMap.end()) {
                    return existing->second;
                }
                LCA_ASSERT(!freeSkeletonSlots.empty(), "AssetManager", "loadSkeleton", "No free skeleton slots available.");
            }

            Assimp::Importer importer;
            const aiScene* scene = importer.ReadFile(filePath,
                aiProcess_Triangulate |
                aiProcess_GenNormals |
                aiProcess_FlipUVs |
                aiProcess_CalcTangentSpace |
                aiProcess_JoinIdenticalVertices);

            LCA_ASSERT(scene && !(scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) && scene->mRootNode,
                "AssetManager", "loadSkeleton", "Failed to load model: " + std::string(importer.GetErrorString()));

            std::unordered_map<std::string, int32_t> boneNameToIndex;
            std::unordered_map<std::string, glm::mat4> boneOffsetByName;
            int32_t nextBoneIndex = 0;

            for (uint32_t m = 0; m < scene->mNumMeshes; ++m) {
                const aiMesh* aiMeshPtr = scene->mMeshes[m];
                for (uint32_t b = 0; b < aiMeshPtr->mNumBones; ++b) {
                    const aiBone* bone = aiMeshPtr->mBones[b];
                    const std::string boneName = bone->mName.C_Str();
                    if (boneNameToIndex.find(boneName) == boneNameToIndex.end()) {
                        LCA_ASSERT(nextBoneIndex < static_cast<int32_t>(MAX_NODES_PER_SKELETON), "AssetManager", "loadSkeleton", "Exceeded max bones per skeleton for '" + name + "'.");
                        boneNameToIndex[boneName] = nextBoneIndex++;
                        boneOffsetByName[boneName] = toGlmMat4(bone->mOffsetMatrix);
                    }
                }
            }

            std::array<VkNode, MAX_NODES_PER_SKELETON> skeletonNodes{};
            for (uint32_t i = 0; i < MAX_NODES_PER_SKELETON; ++i) {
                skeletonNodes[i].isBone_parentIndex_boneIndex = glm::ivec4(0, -1, -1, 0);
                skeletonNodes[i].offset = glm::mat4(1.0f);
            }

            int32_t nextNodeIndex = 0;
            std::unordered_map<std::string, int32_t> nodeNameToIndex;
            std::function<void(aiNode*, int32_t)> fillNodes = [&](aiNode* node, int32_t parentIndex) {
                LCA_ASSERT(nextNodeIndex < static_cast<int32_t>(MAX_NODES_PER_SKELETON), "AssetManager", "loadSkeleton", "Exceeded max nodes (256) for skeleton '" + name + "'.");

                const int32_t nodeIndex = nextNodeIndex++;
                const std::string nodeName = node->mName.C_Str();
                nodeNameToIndex[nodeName] = nodeIndex;
                const auto boneIt = boneNameToIndex.find(nodeName);
                const bool isBone = boneIt != boneNameToIndex.end();
                const int32_t boneIndex = isBone ? boneIt->second : -1;

                skeletonNodes[nodeIndex].isBone_parentIndex_boneIndex = glm::ivec4(isBone ? 1 : 0, parentIndex, boneIndex, 0);
                if (isBone) {
                    skeletonNodes[nodeIndex].offset = boneOffsetByName[nodeName];
                } else {
                    skeletonNodes[nodeIndex].offset = toGlmMat4(node->mTransformation);
                }

                for (uint32_t i = 0; i < node->mNumChildren; ++i) {
                    fillNodes(node->mChildren[i], nodeIndex);
                }
            };

            fillNodes(scene->mRootNode, -1);

            uint32_t skeletonId;
            {
                std::lock_guard<std::mutex> lock(assetMutex);
                LCA_ASSERT(!freeSkeletonSlots.empty(), "AssetManager", "loadSkeleton", "No free skeleton slots available.");
                skeletonId = freeSkeletonSlots.back();
                freeSkeletonSlots.pop_back();
            }

            BufferInterface stagingNodes = createBufferInterface(MAX_NODES_PER_SKELETON, sizeof(VkNode), VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
            std::memcpy(stagingNodes.pMemory, skeletonNodes.data(), MAX_NODES_PER_SKELETON * sizeof(VkNode));

            BufferInterface stagingNodeCount = createBufferInterface(1, sizeof(uint32_t), VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
            const uint32_t nodeCount = static_cast<uint32_t>(nextNodeIndex);
            std::memcpy(stagingNodeCount.pMemory, &nodeCount, sizeof(uint32_t));

            beginSingleCommand(singleCommand);

            VkBufferCopy copyRegion{};
            copyRegion.srcOffset = 0;
            copyRegion.dstOffset = static_cast<VkDeviceSize>(skeletonId) * MAX_NODES_PER_SKELETON * sizeof(VkNode);
            copyRegion.size = MAX_NODES_PER_SKELETON * sizeof(VkNode);
            vkCmdCopyBuffer(singleCommand.vkCommandBuffer, stagingNodes.vkBuffer, skeletonBuffer.vkBuffer, 1, &copyRegion);

            VkBufferCopy nodeCountCopyRegion{};
            nodeCountCopyRegion.srcOffset = 0;
            nodeCountCopyRegion.dstOffset = static_cast<VkDeviceSize>(skeletonId) * sizeof(uint32_t);
            nodeCountCopyRegion.size = sizeof(uint32_t);
            vkCmdCopyBuffer(singleCommand.vkCommandBuffer, stagingNodeCount.vkBuffer, skeletonNodeCountBuffer.vkBuffer, 1, &nodeCountCopyRegion);

            endSingleCommand(singleCommand);
            submitSingleCommand(singleCommand);

            destroyBufferInterface(stagingNodes);
            destroyBufferInterface(stagingNodeCount);

            // Build CPU-side skeleton hierarchy for bone matrix computation
            std::vector<SkeletonNodeInfo> cpuNodes(static_cast<size_t>(nextNodeIndex));
            for (int32_t i = 0; i < nextNodeIndex; ++i) {
                cpuNodes[i].parentIndex = skeletonNodes[i].isBone_parentIndex_boneIndex.y;
                cpuNodes[i].isBone = (skeletonNodes[i].isBone_parentIndex_boneIndex.x != 0);
                cpuNodes[i].offsetMatrix = skeletonNodes[i].offset;
            }

            {
                std::lock_guard<std::mutex> lock(assetMutex);
                skeletonMap[name] = skeletonId;
                SkeletonData skelData;
                skelData.skeletonId = skeletonId;
                skelData.nodeCount = static_cast<uint32_t>(nextNodeIndex);
                skelData.nodeNameToIndex = std::move(nodeNameToIndex);
                skelData.nodes = std::move(cpuNodes);
                skeletonDataMap[name] = std::move(skelData);
            }

            LCA_LOGI("AssetManager", "loadSkeleton", "Loaded skeleton '" + name + "' with " + std::to_string(nextNodeIndex) + " node(s) from " + filePath);
            return skeletonId;
        }

        void AssetManager::loadTexture(const std::string& name, const std::string& filePath) {
            Core::TextureCPU textureCPU = Core::loadTexture(filePath);
            Core::Texture texture = Core::createTexture(textureCPU.width, textureCPU.height, textureCPU.pixels);
            Core::freeTexture(textureCPU);
            addTexture(name, texture);
        }

        void AssetManager::addTexture(const std::string& name, const Texture& texture){
            std::lock_guard<std::mutex> lock(assetMutex);
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
            std::lock_guard<std::mutex> lock(assetMutex);
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
            std::lock_guard<std::mutex> lock(assetMutex);
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
            std::lock_guard<std::mutex> lock(assetMutex);
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
            std::lock_guard<std::mutex> lock(assetMutex);
            if (materialMap.find(name) != materialMap.end()) {
                uint32_t id = materialMap[name];
                materials.remove(id);
                materialMap.erase(name);
                return;
            }
            LCA_LOGE("AssetManager", "removeMaterial", "Material with name " + name + " not found.");
        }

        uint32_t AssetManager::getMaterialId(const std::string& name) const {
            std::lock_guard<std::mutex> lock(assetMutex);
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
            LCA_ASSERT(vertices.size() > 0, "AssetManager", "addMesh",  "Vertex list is empty for mesh " + name + ".");
            LCA_ASSERT(indices.size() > 0, "AssetManager", "addMesh",  "Index list is empty for mesh " + name + ".");

            uint32_t vertexCount = static_cast<uint32_t>(vertices.size());
            uint32_t indexCount = static_cast<uint32_t>(indices.size());

            uint32_t vertexOffsetVal, indexOffsetVal;
            {
                std::lock_guard<std::mutex> lock(assetMutex);
                LCA_ASSERT(meshMap.find(name) == meshMap.end(), "AssetManager", "addMesh",  "Mesh with name " + name + " already exists.");
                vertexOffsetVal = allocateRange(freeVertexRanges, currentVertexOffset, vertexCount);
                indexOffsetVal = allocateRange(freeIndexRanges, currentIndexOffset, indexCount);
            }

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
        
            uint32_t id;
            {
                std::lock_guard<std::mutex> lock(assetMutex);
                id = meshInfos.add(&info);
                meshMap[name] = id;
            }

            return id;
        }

        void AssetManager::removeMesh(const std::string& name){
            std::lock_guard<std::mutex> lock(assetMutex);
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
            std::lock_guard<std::mutex> lock(assetMutex);
            auto it = meshMap.find(name);
            if (it != meshMap.end()) {
                return it->second;
            }
            LCA_LOGE("AssetManager", "getMeshId", "Mesh with name " + name + " not found.");
        }

        Model AssetManager::loadModel(const std::string& name, const std::string& filePath) {
            LCA_ASSERT(modelMap.find(name) == modelMap.end(), "AssetManager", "loadModel", "Model with name " + name + " already exists.");

            Assimp::Importer importer;
            const aiScene* scene = importer.ReadFile(filePath,
                aiProcess_Triangulate |
                aiProcess_GenNormals |
                aiProcess_FlipUVs |
                aiProcess_CalcTangentSpace |
                aiProcess_JoinIdenticalVertices);

            LCA_ASSERT(scene && !(scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) && scene->mRootNode,
                "AssetManager", "loadModel", "Failed to load model: " + std::string(importer.GetErrorString()));

            std::filesystem::path modelPath(filePath);
            std::string directory = modelPath.parent_path().string();

            Model model;

            // --- Pre-process materials (once per unique aiMaterial) ---
            // Maps assimp material index -> our material id / name
            std::unordered_map<uint32_t, uint32_t> processedMaterials;
            std::unordered_map<uint32_t, std::string> processedMaterialNames;

            for (uint32_t matIdx = 0; matIdx < scene->mNumMaterials; ++matIdx) {
                const aiMaterial* aiMat = scene->mMaterials[matIdx];
                aiString aiMatName;
                std::string materialName = (aiMat->Get(AI_MATKEY_NAME, aiMatName) == AI_SUCCESS && aiMatName.length > 0)
                    ? std::string(aiMatName.C_Str())
                    : name + "_material_" + std::to_string(matIdx);

                Material material{};
                material.textureId = -1;
                material.roughness = 0.5f;
                material.metallic = 0.0f;

                // Try to load diffuse texture
                if (aiMat->GetTextureCount(aiTextureType_DIFFUSE) > 0) {
                    aiString texPath;
                    if (aiMat->GetTexture(aiTextureType_DIFFUSE, 0, &texPath) == AI_SUCCESS) {
                        std::string textureName = name + "_tex_" + std::to_string(matIdx);

                        // Only load the texture if it hasn't been loaded yet
                        if (textureMap.find(textureName) == textureMap.end()) {
                            const aiTexture* embeddedTex = scene->GetEmbeddedTexture(texPath.C_Str());
                            if (embeddedTex) {
                                if (embeddedTex->mHeight == 0) {
                                    int width, height, nrChannels;
                                    auto* pixels = stbi_load_from_memory(
                                        reinterpret_cast<unsigned char*>(embeddedTex->pcData),
                                        embeddedTex->mWidth, &width, &height, &nrChannels, 4);
                                    Texture tex = createTexture(width, height, pixels);
                                    stbi_image_free(pixels);
                                    addTexture(textureName, tex);
                                } else {
                                    // Raw ARGB8888 data
                                    int width, height, nrChannels;
                                    auto* pixels = stbi_load_from_memory(
                                    reinterpret_cast<unsigned char*>(embeddedTex->pcData),
                                    embeddedTex->mWidth * embeddedTex->mHeight * 4, &width, &height, &nrChannels, 4);
                                    Texture tex = createTexture(width, height, pixels);
                                    stbi_image_free(pixels);
                                    addTexture(textureName, tex);
                                }
                            } else {
                                // External texture file
                                std::string fullPath = directory + "/" + texPath.C_Str();
                                loadTexture(textureName, fullPath);
                            }
                        }

                        material.textureId = static_cast<int32_t>(getTextureId(textureName));
                    }
                }

                // Extract roughness/metallic if available
                float roughness = 0.5f;
                float metallic = 0.0f;
                if (aiMat->Get(AI_MATKEY_ROUGHNESS_FACTOR, roughness) == AI_SUCCESS) {
                    material.roughness = roughness;
                }
                if (aiMat->Get(AI_MATKEY_METALLIC_FACTOR, metallic) == AI_SUCCESS) {
                    material.metallic = metallic;
                }

                uint32_t materialId = addMaterial(materialName, material);
                processedMaterials[matIdx] = materialId;
                processedMaterialNames[matIdx] = materialName;
            }

            // --- Process each mesh in the scene ---
            for (uint32_t m = 0; m < scene->mNumMeshes; ++m) {
                const aiMesh* aiMeshPtr = scene->mMeshes[m];
                std::string meshName = aiMeshPtr->mName.length > 0
                    ? std::string(aiMeshPtr->mName.C_Str())
                    : name + "_mesh_" + std::to_string(m);

                // --- Extract vertices ---
                std::vector<Vertex::Mesh> vertices;
                vertices.reserve(aiMeshPtr->mNumVertices);

                for (uint32_t v = 0; v < aiMeshPtr->mNumVertices; ++v) {
                    Vertex::Mesh vertex{};
                    vertex.position = glm::vec3(
                        aiMeshPtr->mVertices[v].x,
                        aiMeshPtr->mVertices[v].y,
                        aiMeshPtr->mVertices[v].z
                    );

                    if (aiMeshPtr->HasNormals()) {
                        vertex.normal = glm::vec3(
                            aiMeshPtr->mNormals[v].x,
                            aiMeshPtr->mNormals[v].y,
                            aiMeshPtr->mNormals[v].z
                        );
                    }

                    if (aiMeshPtr->HasVertexColors(0)) {
                        vertex.color = glm::vec4(
                            aiMeshPtr->mColors[0][v].r,
                            aiMeshPtr->mColors[0][v].g,
                            aiMeshPtr->mColors[0][v].b,
                            aiMeshPtr->mColors[0][v].a
                        );
                    } else {
                        vertex.color = glm::vec4(1.0f);
                    }

                    if (aiMeshPtr->HasTextureCoords(0)) {
                        vertex.texCoord = glm::vec2(
                            aiMeshPtr->mTextureCoords[0][v].x,
                            aiMeshPtr->mTextureCoords[0][v].y
                        );
                    }

                    vertices.push_back(vertex);
                }

                // --- Extract indices ---
                std::vector<uint32_t> indices;
                for (uint32_t f = 0; f < aiMeshPtr->mNumFaces; ++f) {
                    const aiFace& face = aiMeshPtr->mFaces[f];
                    indices.push_back(face.mIndices[0]);
                    indices.push_back(face.mIndices[2]);
                    indices.push_back(face.mIndices[1]);
                }

                uint32_t meshId = addMesh(meshName, vertices, indices);

                // Look up the material id from the pre-processed map
                uint32_t materialId = processedMaterials[aiMeshPtr->mMaterialIndex];
                std::string matName = processedMaterialNames[aiMeshPtr->mMaterialIndex];

                model.push_back({ meshName, matName, meshId, materialId });
            }

            modelMap[name] = model;
            LCA_LOGI("AssetManager", "loadModel", "Loaded model '" + name + "' with " + std::to_string(model.size()) + " mesh(es) from " + filePath);
            return model;
        }

        const Model& AssetManager::getModel(const std::string& name) const {
            auto it = modelMap.find(name);
            LCA_ASSERT(it != modelMap.end(), "AssetManager", "getModel", "Model with name " + name + " not found.");
            return it->second;
        }

        uint32_t AssetManager::addSkeletonMesh(const std::string& name, const std::vector<Vertex::Skeleton>& vertices, const std::vector<uint32_t>& indices){
            LCA_ASSERT(vertices.size() > 0, "AssetManager", "addSkeletonMesh",  "Vertex list is empty for skeleton mesh " + name + ".");
            LCA_ASSERT(indices.size() > 0, "AssetManager", "addSkeletonMesh",  "Index list is empty for skeleton mesh " + name + ".");

            uint32_t vertexCount = static_cast<uint32_t>(vertices.size());
            uint32_t indexCount = static_cast<uint32_t>(indices.size());

            uint32_t vertexOffsetVal, indexOffsetVal;
            {
                std::lock_guard<std::mutex> lock(assetMutex);
                LCA_ASSERT(skeletonMeshMap.find(name) == skeletonMeshMap.end(), "AssetManager", "addSkeletonMesh",  "Skeleton mesh with name " + name + " already exists.");
                vertexOffsetVal = allocateRange(freeSkeletonVertexRanges, currentSkeletonVertexOffset, vertexCount);
                indexOffsetVal = allocateRange(freeSkeletonIndexRanges, currentSkeletonIndexOffset, indexCount);
            }

            VkDeviceSize vertexByteSize = vertexCount * sizeof(Vertex::Skeleton);
            BufferInterface stagingVertex = createBufferInterface(vertexCount, sizeof(Vertex::Skeleton), VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
            memcpy(stagingVertex.pMemory, vertices.data(), vertexByteSize);

            VkDeviceSize indexByteSize = indexCount * sizeof(uint32_t);
            BufferInterface stagingIndex = createBufferInterface(indexCount, sizeof(uint32_t), VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
            memcpy(stagingIndex.pMemory, indices.data(), indexByteSize);

            beginSingleCommand(singleCommand);

            VkBufferCopy copyRegion{};
            copyRegion.srcOffset = 0;
            copyRegion.dstOffset = vertexOffsetVal * sizeof(Vertex::Skeleton);
            copyRegion.size = vertexByteSize;
            vkCmdCopyBuffer(singleCommand.vkCommandBuffer, stagingVertex.vkBuffer, skeletonVertexBuffer.vkBuffer, 1, &copyRegion);

            VkBufferCopy copyRegionIndex{};
            copyRegionIndex.srcOffset = 0;
            copyRegionIndex.dstOffset = indexOffsetVal * sizeof(uint32_t);
            copyRegionIndex.size = indexByteSize;
            vkCmdCopyBuffer(singleCommand.vkCommandBuffer, stagingIndex.vkBuffer, skeletonIndexBuffer.vkBuffer, 1, &copyRegionIndex);

            endSingleCommand(singleCommand);
            submitSingleCommand(singleCommand);

            destroyBufferInterface(stagingIndex);
            destroyBufferInterface(stagingVertex);

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

            uint32_t id;
            {
                std::lock_guard<std::mutex> lock(assetMutex);
                id = skeletonMeshInfos.add(&info);
                skeletonMeshMap[name] = id;
            }

            return id;
        }

        void AssetManager::removeSkeletonMesh(const std::string& name){
            std::lock_guard<std::mutex> lock(assetMutex);
            auto it = skeletonMeshMap.find(name);
            if (it != skeletonMeshMap.end()) {
                uint32_t id = it->second;

                MeshInfo info;
                skeletonMeshInfos.remove(id, &info);

                freeRange(freeSkeletonVertexRanges, static_cast<uint32_t>(info.vertexOffset), info.vertexCount);
                freeRange(freeSkeletonIndexRanges, info.firstIndex, info.indexCount);

                skeletonMeshMap.erase(name);
                return;
            }
            LCA_LOGE("AssetManager", "removeSkeletonMesh", "Skeleton mesh with name " + name + " not found.");
        }

        Model AssetManager::loadSkeletonModel(const std::string& name, const std::string& filePath) {
            LCA_ASSERT(skeletonModelMap.find(name) == skeletonModelMap.end(), "AssetManager", "loadSkeletonModel", "Skeleton model with name " + name + " already exists.");

            loadSkeleton(name, filePath);

            Assimp::Importer importer;
            const aiScene* scene = importer.ReadFile(filePath,
                aiProcess_Triangulate |
                aiProcess_GenNormals |
                aiProcess_FlipUVs |
                aiProcess_CalcTangentSpace |
                aiProcess_JoinIdenticalVertices);

            LCA_ASSERT(scene && !(scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) && scene->mRootNode,
                "AssetManager", "loadSkeletonModel", "Failed to load model: " + std::string(importer.GetErrorString()));

            std::filesystem::path modelPath(filePath);
            std::string directory = modelPath.parent_path().string();

            Model model;

            // --- Pre-process materials ---
            std::unordered_map<uint32_t, uint32_t> processedMaterials;
            std::unordered_map<uint32_t, std::string> processedMaterialNames;

            for (uint32_t matIdx = 0; matIdx < scene->mNumMaterials; ++matIdx) {
                const aiMaterial* aiMat = scene->mMaterials[matIdx];
                aiString aiMatName;
                std::string materialName = (aiMat->Get(AI_MATKEY_NAME, aiMatName) == AI_SUCCESS && aiMatName.length > 0)
                    ? std::string(aiMatName.C_Str())
                    : name + "_material_" + std::to_string(matIdx);

                Material material{};
                material.textureId = -1;
                material.roughness = 0.5f;
                material.metallic = 0.0f;

                if (aiMat->GetTextureCount(aiTextureType_DIFFUSE) > 0) {
                    aiString texPath;
                    if (aiMat->GetTexture(aiTextureType_DIFFUSE, 0, &texPath) == AI_SUCCESS) {
                        std::string textureName = name + "_tex_" + std::to_string(matIdx);

                        if (textureMap.find(textureName) == textureMap.end()) {
                            const aiTexture* embeddedTex = scene->GetEmbeddedTexture(texPath.C_Str());
                            if (embeddedTex) {
                                if (embeddedTex->mHeight == 0) {
                                    int width, height, nrChannels;
                                    auto* pixels = stbi_load_from_memory(
                                        reinterpret_cast<unsigned char*>(embeddedTex->pcData),
                                        embeddedTex->mWidth, &width, &height, &nrChannels, 4);
                                    Texture tex = createTexture(width, height, pixels);
                                    stbi_image_free(pixels);
                                    addTexture(textureName, tex);
                                } else {
                                    int width, height, nrChannels;
                                    auto* pixels = stbi_load_from_memory(
                                        reinterpret_cast<unsigned char*>(embeddedTex->pcData),
                                        embeddedTex->mWidth * embeddedTex->mHeight * 4, &width, &height, &nrChannels, 4);
                                    Texture tex = createTexture(width, height, pixels);
                                    stbi_image_free(pixels);
                                    addTexture(textureName, tex);
                                }
                            } else {
                                std::string fullPath = directory + "/" + texPath.C_Str();
                                loadTexture(textureName, fullPath);
                            }
                        }

                        material.textureId = static_cast<int32_t>(getTextureId(textureName));
                    }
                }

                float roughness = 0.5f;
                float metallic = 0.0f;
                if (aiMat->Get(AI_MATKEY_ROUGHNESS_FACTOR, roughness) == AI_SUCCESS) {
                    material.roughness = roughness;
                }
                if (aiMat->Get(AI_MATKEY_METALLIC_FACTOR, metallic) == AI_SUCCESS) {
                    material.metallic = metallic;
                }

                uint32_t materialId = addMaterial(materialName, material);
                processedMaterials[matIdx] = materialId;
                processedMaterialNames[matIdx] = materialName;
            }

            // --- Build node index map for skeleton ---
            std::unordered_map<std::string, int> nodeIndexMap;
            std::function<void(aiNode*, int&)> buildNodeMap = [&](aiNode* node, int& nextIndex) {
                nodeIndexMap[node->mName.C_Str()] = nextIndex++;
                for (uint32_t i = 0; i < node->mNumChildren; ++i) {
                    buildNodeMap(node->mChildren[i], nextIndex);
                }
            };
            int nextNodeIndex = 0;
            buildNodeMap(scene->mRootNode, nextNodeIndex);

            // Helper to find the parent node that references a mesh by index
            std::function<aiNode*(aiNode*, uint32_t)> findMeshParentNode = [&](aiNode* node, uint32_t meshIndex) -> aiNode* {
                for (uint32_t i = 0; i < node->mNumMeshes; ++i) {
                    if (node->mMeshes[i] == meshIndex) {
                        return node;
                    }
                }
                for (uint32_t i = 0; i < node->mNumChildren; ++i) {
                    aiNode* found = findMeshParentNode(node->mChildren[i], meshIndex);
                    if (found != nullptr) {
                        return found;
                    }
                }
                return nullptr;
            };

            // --- Process each mesh ---
            for (uint32_t m = 0; m < scene->mNumMeshes; ++m) {
                const aiMesh* aiMeshPtr = scene->mMeshes[m];
                std::string meshName = aiMeshPtr->mName.length > 0
                    ? std::string(aiMeshPtr->mName.C_Str())
                    : name + "_mesh_" + std::to_string(m);

                int32_t meshNodeIndex = -1; // -1 for deformed, >= 0 for node-attached
                std::vector<Vertex::Skeleton> vertices;
                vertices.resize(aiMeshPtr->mNumVertices);

                for (uint32_t v = 0; v < aiMeshPtr->mNumVertices; ++v) {
                    Vertex::Skeleton& vertex = vertices[v];
                    vertex.position = glm::vec3(
                        aiMeshPtr->mVertices[v].x,
                        aiMeshPtr->mVertices[v].y,
                        aiMeshPtr->mVertices[v].z
                    );

                    if (aiMeshPtr->HasNormals()) {
                        vertex.normal = glm::vec3(
                            aiMeshPtr->mNormals[v].x,
                            aiMeshPtr->mNormals[v].y,
                            aiMeshPtr->mNormals[v].z
                        );
                    }

                    if (aiMeshPtr->HasVertexColors(0)) {
                        vertex.color = glm::vec4(
                            aiMeshPtr->mColors[0][v].r,
                            aiMeshPtr->mColors[0][v].g,
                            aiMeshPtr->mColors[0][v].b,
                            aiMeshPtr->mColors[0][v].a
                        );
                    } else {
                        vertex.color = glm::vec4(1.0f);
                    }

                    if (aiMeshPtr->HasTextureCoords(0)) {
                        vertex.texCoord = glm::vec2(
                            aiMeshPtr->mTextureCoords[0][v].x,
                            aiMeshPtr->mTextureCoords[0][v].y
                        );
                    }

                    vertex.bones = glm::ivec4(0);
                    vertex.weights = glm::vec4(0.0f);
                    vertex.nodeIndex = -1;
                }

                // --- Extract bone data or set nodeIndex from parent node ---
                if (aiMeshPtr->HasBones()) {
                    for (uint32_t b = 0; b < aiMeshPtr->mNumBones; ++b) {
                        const aiBone* bone = aiMeshPtr->mBones[b];
                        std::string boneName(bone->mName.C_Str());
                        auto boneIt = nodeIndexMap.find(boneName);
                        LCA_ASSERT(boneIt != nodeIndexMap.end(), "AssetManager", "loadSkeletonModel",
                            "Bone '" + boneName + "' not found in node index map.");
                        int boneIndex = boneIt->second;

                        for (uint32_t w = 0; w < bone->mNumWeights; ++w) {
                            uint32_t vertexId = bone->mWeights[w].mVertexId;
                            float weight = bone->mWeights[w].mWeight;

                            Vertex::Skeleton& vert = vertices[vertexId];

                            // Find the first empty bone slot (weight == 0)
                            for (int slot = 0; slot < 4; ++slot) {
                                if (vert.weights[slot] == 0.0f) {
                                    vert.bones[slot] = boneIndex;
                                    vert.weights[slot] = weight;
                                    vert.nodeIndex = -1;
                                    break;
                                }
                            }
                        }
                    }
                } else {
                    // No bones — use parent node's global transform via the bone skinning path
                    aiNode* parentNode = findMeshParentNode(scene->mRootNode, m);
                    LCA_ASSERT(parentNode != nullptr, "AssetManager", "loadSkeletonModel", "Parent node not found for mesh: " + std::to_string(m));

                    std::string nodeName(parentNode->mName.C_Str());
                    LCA_ASSERT(nodeIndexMap.find(nodeName) != nodeIndexMap.end(), "AssetManager", "loadSkeletonModel", "Node not found in node mapping: " + nodeName);

                    meshNodeIndex = nodeIndexMap[nodeName];
                    for (uint32_t v = 0; v < aiMeshPtr->mNumVertices; ++v) {
                        vertices[v].nodeIndex = meshNodeIndex;
                        vertices[v].bones = glm::ivec4(meshNodeIndex, 0, 0, 0);
                        vertices[v].weights = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);
                    }
                }

                // --- Extract indices ---
                std::vector<uint32_t> indices;
                for (uint32_t f = 0; f < aiMeshPtr->mNumFaces; ++f) {
                    const aiFace& face = aiMeshPtr->mFaces[f];
                    indices.push_back(face.mIndices[0]);
                    indices.push_back(face.mIndices[2]);
                    indices.push_back(face.mIndices[1]);
                }

                uint32_t meshId = addSkeletonMesh(meshName, vertices, indices);

                uint32_t materialId = processedMaterials[aiMeshPtr->mMaterialIndex];
                std::string matName = processedMaterialNames[aiMeshPtr->mMaterialIndex];

                model.push_back({ meshName, matName, meshId, materialId, meshNodeIndex });
            }

            skeletonModelMap[name] = model;
            LCA_LOGI("AssetManager", "loadSkeletonModel", "Loaded skeleton model '" + name + "' with " + std::to_string(model.size()) + " mesh(es) from " + filePath);
            return model;
        }

        std::vector<std::string> AssetManager::loadAnimations(const std::string& prefix, const std::string& filePath) {
            Assimp::Importer importer;
            const aiScene* scene = importer.ReadFile(filePath,
                aiProcess_Triangulate |
                aiProcess_GenNormals |
                aiProcess_FlipUVs |
                aiProcess_CalcTangentSpace |
                aiProcess_JoinIdenticalVertices);

            LCA_ASSERT(scene && !(scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) && scene->mRootNode,
                "AssetManager", "loadAnimations", "Failed to load file: " + std::string(importer.GetErrorString()));

            LCA_ASSERT(scene->mNumAnimations > 0,
                "AssetManager", "loadAnimations", "No animations found in " + filePath);

            std::vector<std::string> loadedNames;
            loadedNames.reserve(scene->mNumAnimations);

            for (uint32_t a = 0; a < scene->mNumAnimations; ++a) {
                const aiAnimation* aiAnim = scene->mAnimations[a];

                std::string animName = prefix + "/" +
                    (aiAnim->mName.length > 0
                        ? std::string(aiAnim->mName.C_Str())
                        : std::to_string(a));

                {
                    std::lock_guard<std::mutex> lock(assetMutex);
                    if (animationMap.find(animName) != animationMap.end()) {
                        loadedNames.push_back(animName);
                        continue;
                    }
                }

                Animation animation;
                animation.name = animName;
                animation.duration = static_cast<float>(aiAnim->mDuration);
                animation.ticksPerSecond = aiAnim->mTicksPerSecond > 0.0
                    ? static_cast<float>(aiAnim->mTicksPerSecond)
                    : 25.0f;

                // Pre-count total keyframes for reservation
                uint32_t totalPos = 0, totalRot = 0, totalScale = 0;
                for (uint32_t c = 0; c < aiAnim->mNumChannels; ++c) {
                    const aiNodeAnim* ch = aiAnim->mChannels[c];
                    totalPos += ch->mNumPositionKeys;
                    totalRot += ch->mNumRotationKeys;
                    totalScale += ch->mNumScalingKeys;
                }

                animation.positionKeys.reserve(totalPos);
                animation.rotationKeys.reserve(totalRot);
                animation.scaleKeys.reserve(totalScale);
                animation.channels.reserve(aiAnim->mNumChannels);

                for (uint32_t c = 0; c < aiAnim->mNumChannels; ++c) {
                    const aiNodeAnim* aiChannel = aiAnim->mChannels[c];

                    AnimationChannel channel;
                    channel.nodeName = aiChannel->mNodeName.C_Str();

                    // Position keyframes
                    channel.positionOffset = static_cast<uint32_t>(animation.positionKeys.size());
                    channel.positionCount = aiChannel->mNumPositionKeys;
                    for (uint32_t k = 0; k < aiChannel->mNumPositionKeys; ++k) {
                        const auto& key = aiChannel->mPositionKeys[k];
                        animation.positionKeys.push_back({
                            static_cast<float>(key.mTime),
                            glm::vec3(key.mValue.x, key.mValue.y, key.mValue.z)
                        });
                    }

                    // Rotation keyframes
                    channel.rotationOffset = static_cast<uint32_t>(animation.rotationKeys.size());
                    channel.rotationCount = aiChannel->mNumRotationKeys;
                    for (uint32_t k = 0; k < aiChannel->mNumRotationKeys; ++k) {
                        const auto& key = aiChannel->mRotationKeys[k];
                        animation.rotationKeys.push_back({
                            static_cast<float>(key.mTime),
                            glm::quat(key.mValue.w, key.mValue.x, key.mValue.y, key.mValue.z)
                        });
                    }

                    // Scale keyframes
                    channel.scaleOffset = static_cast<uint32_t>(animation.scaleKeys.size());
                    channel.scaleCount = aiChannel->mNumScalingKeys;
                    for (uint32_t k = 0; k < aiChannel->mNumScalingKeys; ++k) {
                        const auto& key = aiChannel->mScalingKeys[k];
                        animation.scaleKeys.push_back({
                            static_cast<float>(key.mTime),
                            glm::vec3(key.mValue.x, key.mValue.y, key.mValue.z)
                        });
                    }

                    animation.channels.push_back(std::move(channel));
                }

                {
                    std::lock_guard<std::mutex> lock(assetMutex);
                    animationMap[animName] = std::move(animation);
                }

                loadedNames.push_back(animName);
                LCA_LOGI("AssetManager", "loadAnimations", "Loaded animation '" + animName + "' with " + std::to_string(aiAnim->mNumChannels) + " channel(s)");
            }

            return loadedNames;
        }

        const Animation& AssetManager::getAnimation(const std::string& name) const {
            std::lock_guard<std::mutex> lock(assetMutex);
            auto it = animationMap.find(name);
            LCA_ASSERT(it != animationMap.end(), "AssetManager", "getAnimation", "Animation with name " + name + " not found.");
            return it->second;
        }

        void AssetManager::removeAnimation(const std::string& name) {
            std::lock_guard<std::mutex> lock(assetMutex);
            auto it = animationMap.find(name);
            if (it != animationMap.end()) {
                animationMap.erase(it);
                return;
            }
            LCA_LOGE("AssetManager", "removeAnimation", "Animation with name " + name + " not found.");
        }

        bool AssetManager::isAnimationCompatible(const std::string& skeletonName, const std::string& animationName) const {
            std::lock_guard<std::mutex> lock(assetMutex);
            auto skelIt = skeletonDataMap.find(skeletonName);
            if (skelIt == skeletonDataMap.end()) return false;

            auto animIt = animationMap.find(animationName);
            if (animIt == animationMap.end()) return false;

            const auto& nodeMap = skelIt->second.nodeNameToIndex;
            for (const auto& channel : animIt->second.channels) {
                if (nodeMap.find(channel.nodeName) == nodeMap.end()) {
                    return false;
                }
            }
            return true;
        }

        bool AssetManager::bindAnimationToSkeleton(const std::string& animationName, const std::string& skeletonName) {
            std::lock_guard<std::mutex> lock(assetMutex);
            auto animIt = animationMap.find(animationName);
            LCA_ASSERT(animIt != animationMap.end(), "AssetManager", "bindAnimationToSkeleton", "Animation '" + animationName + "' not found.");

            auto skelIt = skeletonDataMap.find(skeletonName);
            LCA_ASSERT(skelIt != skeletonDataMap.end(), "AssetManager", "bindAnimationToSkeleton", "Skeleton '" + skeletonName + "' not found.");

            const auto& nodeMap = skelIt->second.nodeNameToIndex;
            Animation& anim = animIt->second;

            for (auto& channel : anim.channels) {
                auto nodeIt = nodeMap.find(channel.nodeName);
                if (nodeIt == nodeMap.end()) {
                    // Incompatible — reset all indices and return false
                    for (auto& ch : anim.channels) ch.nodeIndex = -1;
                    return false;
                }
                channel.nodeIndex = nodeIt->second;
            }
            return true;
        }

        std::string AssetManager::extractAnimationForSkeleton(const std::string& animationName, const std::string& skeletonName) {
            std::lock_guard<std::mutex> lock(assetMutex);

            std::string key = skeletonName + ":" + animationName;
            if (animationMap.find(key) != animationMap.end()) return key;

            auto animIt = animationMap.find(animationName);
            LCA_ASSERT(animIt != animationMap.end(), "AssetManager", "extractAnimationForSkeleton", "Animation '" + animationName + "' not found.");

            auto skelIt = skeletonDataMap.find(skeletonName);
            LCA_ASSERT(skelIt != skeletonDataMap.end(), "AssetManager", "extractAnimationForSkeleton", "Skeleton '" + skeletonName + "' not found.");

            const Animation& src = animIt->second;
            const auto& nodeMap = skelIt->second.nodeNameToIndex;

            // Build lookup: nodeName -> source channel index
            std::unordered_map<std::string, uint32_t> channelByName;
            for (uint32_t i = 0; i < static_cast<uint32_t>(src.channels.size()); ++i) {
                channelByName[src.channels[i].nodeName] = i;
            }

            // Find max node index to determine total node count
            uint32_t nodeCount = 0;
            for (const auto& [name, idx] : nodeMap) {
                if (static_cast<uint32_t>(idx) >= nodeCount)
                    nodeCount = static_cast<uint32_t>(idx) + 1;
            }

            Animation result;
            result.name = key;
            result.duration = src.duration;
            result.ticksPerSecond = src.ticksPerSecond;
            result.channels.resize(nodeCount);

            // Reserve approximate space
            result.positionKeys.reserve(src.positionKeys.size() + nodeCount);
            result.rotationKeys.reserve(src.rotationKeys.size() + nodeCount);
            result.scaleKeys.reserve(src.scaleKeys.size() + nodeCount);

            // Sort nodes by index so we iterate 0, 1, 2, ...
            std::vector<std::pair<std::string, int32_t>> sortedNodes(nodeMap.begin(), nodeMap.end());
            std::sort(sortedNodes.begin(), sortedNodes.end(), [](const auto& a, const auto& b) {
                return a.second < b.second;
            });

            for (const auto& [nodeName, nodeIdx] : sortedNodes) {
                AnimationChannel& dst = result.channels[nodeIdx];
                dst.nodeName = nodeName;
                dst.nodeIndex = nodeIdx;

                auto chIt = channelByName.find(nodeName);
                if (chIt != channelByName.end()) {
                    const AnimationChannel& srcCh = src.channels[chIt->second];

                    dst.positionOffset = static_cast<uint32_t>(result.positionKeys.size());
                    dst.positionCount  = srcCh.positionCount;
                    result.positionKeys.insert(result.positionKeys.end(),
                        src.positionKeys.begin() + srcCh.positionOffset,
                        src.positionKeys.begin() + srcCh.positionOffset + srcCh.positionCount);

                    dst.rotationOffset = static_cast<uint32_t>(result.rotationKeys.size());
                    dst.rotationCount  = srcCh.rotationCount;
                    result.rotationKeys.insert(result.rotationKeys.end(),
                        src.rotationKeys.begin() + srcCh.rotationOffset,
                        src.rotationKeys.begin() + srcCh.rotationOffset + srcCh.rotationCount);

                    dst.scaleOffset = static_cast<uint32_t>(result.scaleKeys.size());
                    dst.scaleCount  = srcCh.scaleCount;
                    result.scaleKeys.insert(result.scaleKeys.end(),
                        src.scaleKeys.begin() + srcCh.scaleOffset,
                        src.scaleKeys.begin() + srcCh.scaleOffset + srcCh.scaleCount);
                } else {
                    // No animation data for this node
                    const auto& nodeInfo = skelIt->second.nodes[nodeIdx];
                    if (!nodeInfo.isBone) {
                        // Non-bone: use stored local transform as rest-pose keyframes
                        glm::vec3 scale, translation, skew;
                        glm::vec4 perspective;
                        glm::quat rotation;
                        glm::decompose(nodeInfo.offsetMatrix, scale, rotation, translation, skew, perspective);

                        dst.positionOffset = static_cast<uint32_t>(result.positionKeys.size());
                        dst.positionCount  = 1;
                        result.positionKeys.push_back({0.0f, translation});

                        dst.rotationOffset = static_cast<uint32_t>(result.rotationKeys.size());
                        dst.rotationCount  = 1;
                        result.rotationKeys.push_back({0.0f, rotation});

                        dst.scaleOffset = static_cast<uint32_t>(result.scaleKeys.size());
                        dst.scaleCount  = 1;
                        result.scaleKeys.push_back({0.0f, scale});
                    } else {
                        // Bone without animation: identity keyframes
                        dst.positionOffset = static_cast<uint32_t>(result.positionKeys.size());
                        dst.positionCount  = 1;
                        result.positionKeys.push_back({0.0f, glm::vec3(0.0f)});

                        dst.rotationOffset = static_cast<uint32_t>(result.rotationKeys.size());
                        dst.rotationCount  = 1;
                        result.rotationKeys.push_back({0.0f, glm::quat(1.0f, 0.0f, 0.0f, 0.0f)});

                        dst.scaleOffset = static_cast<uint32_t>(result.scaleKeys.size());
                        dst.scaleCount  = 1;
                        result.scaleKeys.push_back({0.0f, glm::vec3(1.0f)});
                    }
                }
            }

            animationMap[key] = std::move(result);
            LCA_LOGI("AssetManager", "extractAnimationForSkeleton", "Extracted animation '" + animationName + "' for skeleton '" + skeletonName + "' as '" + key + "'");
            return key;
        }

        const SkeletonData& AssetManager::getSkeletonData(const std::string& name) const {
            std::lock_guard<std::mutex> lock(assetMutex);
            auto it = skeletonDataMap.find(name);
            LCA_ASSERT(it != skeletonDataMap.end(), "AssetManager", "getSkeletonData", "Skeleton data with name " + name + " not found.");
            return it->second;
        }
    }
}