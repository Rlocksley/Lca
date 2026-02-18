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

            Pipeline& addUniformBuffer(const uint32_t binding, const BufferInterface& buffer, const VkShaderStageFlags stageFlags);
            Pipeline& addUniformBuffer(const uint32_t binding, const Buffer& buffer, const VkShaderStageFlags stageFlags);
            Pipeline& addStorageBuffer(const uint32_t binding, const BufferInterface& buffer, const VkShaderStageFlags stageFlags);
            Pipeline& addStorageBuffer(const uint32_t binding, const Buffer& buffer, const VkShaderStageFlags stageFlags);
            Pipeline& addBufferArray(const uint32_t binding, const std::vector<Buffer>& buffers, const VkShaderStageFlags stageFlags);
            Pipeline& addBufferArray(const uint32_t binding, const std::vector<Buffer>& buffers, const uint32_t descriptorCount, const VkBuffer fallbackBuffer, const VkShaderStageFlags stageFlags);
            template<size_t N>
            Pipeline& addTextureArray(const uint32_t binding, const std::array<Texture, N>& textures, const VkShaderStageFlags stageFlags){
                LCA_ASSERT(textures.size() > 0, "Pipeline", "addTextureArray", "Texture array must contain at least one texture.")
                return addTextureArrayInternal(binding, textures.data(), static_cast<uint32_t>(textures.size()), stageFlags);
            }

            virtual void build(); //Just build the pipeline layout and descriptor sets, not the pipeline itself
            void updateDescriptorSetWrites();

            const VkDescriptorSet& getVkDescriptorSet() const { return descriptorSet; }
            const VkPipelineLayout& getVkPipelineLayout() const { return pipelineLayout; }
            const VkPipeline& getVkPipeline() const { return pipeline; }

        private:
            struct DescriptorResource {
                uint32_t binding = 0;
                VkDescriptorType type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                VkShaderStageFlags stageFlags = 0;
                VkBuffer buffer = VK_NULL_HANDLE;
                std::vector<VkBuffer> buffers;
                const std::vector<Buffer>* bufferSource = nullptr;
                VkBuffer fallbackBuffer = VK_NULL_HANDLE;
                uint32_t descriptorCount = 1;
                const Texture* textures = nullptr;
            };

            Pipeline& addTextureArrayInternal(const uint32_t binding, const Texture* textures, uint32_t textureCount, const VkShaderStageFlags stageFlags);

            Pipeline& addUniformBuffer(const uint32_t binding, VkBuffer buffer, const VkShaderStageFlags stageFlags);
            Pipeline& addStorageBuffer(const uint32_t binding, VkBuffer buffer, const VkShaderStageFlags stageFlags);

            void resetBaseVulkanObjects();
            void destroyOwnedVulkanObjects();

            std::vector<DescriptorResource> descriptorResources;

            VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
            VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
            VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
            VkPipeline pipeline = VK_NULL_HANDLE;

        protected:
            VkPipeline& pipelineHandle() { return pipeline; }
        };

    }
}