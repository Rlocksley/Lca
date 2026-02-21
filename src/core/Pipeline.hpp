#pragma once

#include "Global.hpp"
#include "Buffer.hpp"

namespace Lca{
    namespace Core{

    struct Texture;

        


        class Pipeline{
        public:
            Pipeline();
            virtual ~Pipeline();

            Pipeline& addUniformBuffer(const uint32_t descriptorSetIndex, const uint32_t binding, const BufferInterface& buffer, const VkShaderStageFlags stageFlags);
            Pipeline& addUniformBuffer(const uint32_t descriptorSetIndex, const uint32_t binding, const Buffer& buffer, const VkShaderStageFlags stageFlags);
            Pipeline& addStorageBuffer(const uint32_t descriptorSetIndex, const uint32_t binding, const BufferInterface& buffer, const VkShaderStageFlags stageFlags);
            Pipeline& addStorageBuffer(const uint32_t descriptorSetIndex, const uint32_t binding, const Buffer& buffer, const VkShaderStageFlags stageFlags);
            Pipeline& addBufferArray(const uint32_t descriptorSetIndex, const uint32_t binding, const std::vector<Buffer>& buffers, const VkShaderStageFlags stageFlags);
            Pipeline& addBufferArray(const uint32_t descriptorSetIndex, const uint32_t binding, const std::vector<Buffer>& buffers, const uint32_t descriptorCount, const VkBuffer fallbackBuffer, const VkShaderStageFlags stageFlags);

            template<size_t N>
            Pipeline& addTextureArray(const uint32_t binding, const std::array<Texture, N>& textures, const VkShaderStageFlags stageFlags){
                LCA_ASSERT(textures.size() > 0, "Pipeline", "addTextureArray", "Texture array must contain at least one texture.")
                return addTextureArrayInternal(binding, textures.data(), static_cast<uint32_t>(textures.size()), stageFlags);
            }

            virtual void build(); //Just build the pipeline layout and descriptor sets, not the pipeline itself
            void updateDescriptorSetWrites();
            void updateDescriptorSetWrites(uint32_t frameIndex);

            const VkDescriptorSet& getVkDescriptorSet(uint32_t frameIndex) const { return descriptorSets[frameIndex]; }
            const VkPipelineLayout& getVkPipelineLayout() const { return pipelineLayout; }
            const VkPipeline& getVkPipeline() const { return pipeline; }

        private:
            struct DescriptorBufferResource {
                uint32_t binding = 0;
                VkDescriptorType type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                VkShaderStageFlags stageFlags = 0;
                VkBuffer buffer = VK_NULL_HANDLE;
                std::vector<VkBuffer> buffers;
                const std::vector<Buffer>* bufferSource = nullptr;
                VkBuffer fallbackBuffer = VK_NULL_HANDLE;
                uint32_t descriptorCount = 1;
            };

            struct DescriptorTextureResource {
                uint32_t binding = 0;
                VkDescriptorType type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                VkShaderStageFlags stageFlags = 0;
                const Texture* textures = nullptr;
                uint32_t descriptorCount = 1;
            };

            Pipeline& addTextureArrayInternal(const uint32_t binding, const Texture* textures, uint32_t textureCount, const VkShaderStageFlags stageFlags);

            Pipeline& addUniformBuffer(const uint32_t descriptorSetIndex, const uint32_t binding, VkBuffer buffer, const VkShaderStageFlags stageFlags);
            Pipeline& addStorageBuffer(const uint32_t descriptorSetIndex, const uint32_t binding, VkBuffer buffer, const VkShaderStageFlags stageFlags);

            void resetBaseVulkanObjects();
            void destroyOwnedVulkanObjects();

            std::array<std::vector<DescriptorBufferResource>, MAX_FRAMES_IN_FLIGHT> descriptorBufferResources;
            std::vector<DescriptorTextureResource> descriptorTextureResources;

            VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
            std::array<VkDescriptorSet, MAX_FRAMES_IN_FLIGHT> descriptorSets{}; // One descriptor set per frame in flight
            VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
            VkPipeline pipeline = VK_NULL_HANDLE;

        protected:
            VkPipeline& pipelineHandle() { return pipeline; }
        };

    }
}