#include "Pipeline.hpp"

#include "DescriptorPool.hpp"
#include "Device.hpp"
#include "Image.hpp"

namespace Lca {
    namespace Core {

        Pipeline::Pipeline() {}

        Pipeline::~Pipeline() {
            destroyOwnedVulkanObjects();
        }

        Pipeline& Pipeline::addUniformBuffer(const uint32_t descriptorSetIndex, const uint32_t binding, const BufferInterface& buffer, const VkShaderStageFlags stageFlags) {
            return addUniformBuffer(descriptorSetIndex, binding, buffer.vkBuffer, stageFlags);
        }

        Pipeline& Pipeline::addUniformBuffer(const uint32_t descriptorSetIndex, const uint32_t binding, const Buffer& buffer, const VkShaderStageFlags stageFlags) {
            return addUniformBuffer(descriptorSetIndex, binding, buffer.vkBuffer, stageFlags);
        }

        Pipeline& Pipeline::addStorageBuffer(const uint32_t descriptorSetIndex, const uint32_t binding, const BufferInterface& buffer, const VkShaderStageFlags stageFlags) {
            return addStorageBuffer(descriptorSetIndex, binding, buffer.vkBuffer, stageFlags);
        }

        Pipeline& Pipeline::addStorageBuffer(const uint32_t descriptorSetIndex, const uint32_t binding, const Buffer& buffer, const VkShaderStageFlags stageFlags) {
            return addStorageBuffer(descriptorSetIndex, binding, buffer.vkBuffer, stageFlags);
        }

        Pipeline& Pipeline::addBufferArray(const uint32_t descriptorSetIndex, const uint32_t binding, const std::vector<Buffer>& buffers, const VkShaderStageFlags stageFlags) {
            LCA_ASSERT(!buffers.empty(), "Pipeline", "addBufferArray", "Buffer array must contain at least one buffer.")

            DescriptorBufferResource resource{};
            resource.binding = binding;
            resource.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            resource.stageFlags = stageFlags;
            resource.buffer = VK_NULL_HANDLE;
            resource.buffers.reserve(buffers.size());
            for (const auto& buffer : buffers) {
                resource.buffers.push_back(buffer.vkBuffer);
            }
            resource.bufferSource = nullptr;
            resource.fallbackBuffer = VK_NULL_HANDLE;
            resource.descriptorCount = static_cast<uint32_t>(resource.buffers.size());
            descriptorBufferResources[descriptorSetIndex].push_back(resource);
            return *this;
        }

        Pipeline& Pipeline::addBufferArray(const uint32_t descriptorSetIndex, const uint32_t binding, const std::vector<Buffer>& buffers, const uint32_t descriptorCount, const VkBuffer fallbackBuffer, const VkShaderStageFlags stageFlags) {
            LCA_ASSERT(descriptorCount > 0, "Pipeline", "addBufferArray", "Descriptor count must be greater than zero.")
            LCA_ASSERT(fallbackBuffer != VK_NULL_HANDLE, "Pipeline", "addBufferArray", "Fallback buffer must be valid.")

            DescriptorBufferResource resource{};
            resource.binding = binding;
            resource.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            resource.stageFlags = stageFlags;
            resource.buffer = VK_NULL_HANDLE;
            resource.buffers.clear();
            resource.bufferSource = &buffers;
            resource.fallbackBuffer = fallbackBuffer;
            resource.descriptorCount = descriptorCount;
            descriptorBufferResources[descriptorSetIndex].push_back(resource);
            return *this;
        }

        Pipeline& Pipeline::addUniformBuffer(const uint32_t descriptorSetIndex, const uint32_t binding, VkBuffer buffer, const VkShaderStageFlags stageFlags) {
            DescriptorBufferResource resource{};
            resource.binding = binding;
            resource.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            resource.stageFlags = stageFlags;
            resource.buffer = buffer;
            resource.descriptorCount = 1;
            descriptorBufferResources[descriptorSetIndex].push_back(resource);
            return *this;
        }

        Pipeline& Pipeline::addStorageBuffer(const uint32_t descriptorSetIndex, const uint32_t binding, VkBuffer buffer, const VkShaderStageFlags stageFlags) {
            DescriptorBufferResource resource{};
            resource.binding = binding;
            resource.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            resource.stageFlags = stageFlags;
            resource.buffer = buffer;
            resource.descriptorCount = 1;
            descriptorBufferResources[descriptorSetIndex].push_back(resource);
            return *this;
        }

        Pipeline& Pipeline::addTextureArrayInternal(const uint32_t binding, const Texture* textures, uint32_t textureCount, const VkShaderStageFlags stageFlags) {
            LCA_ASSERT(textureCount > 0, "Pipeline", "addTextureArray", "Texture array must contain at least one texture.")
            DescriptorTextureResource resource{};
            resource.binding = binding;
            resource.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            resource.stageFlags = stageFlags;
            resource.textures = textures;
            resource.descriptorCount = textureCount;
            descriptorTextureResources.push_back(resource);
            return *this;
        }

        void Pipeline::resetBaseVulkanObjects() {
            
            for(auto& descriptorSet : descriptorSets) {
                if (descriptorSet != VK_NULL_HANDLE) {
                        LCA_CHECK_VULKAN(
                            vkFreeDescriptorSets(vkDevice, vkDescriptorPool, 1, &descriptorSet),
                            "Pipeline::resetBaseVulkanObjects",
                            "vkFreeDescriptorSets"
                        )
                    descriptorSet = VK_NULL_HANDLE;
                }
            }

            if (pipelineLayout != VK_NULL_HANDLE) {
                vkDestroyPipelineLayout(vkDevice, pipelineLayout, nullptr);
                pipelineLayout = VK_NULL_HANDLE;
            }

            if (descriptorSetLayout != VK_NULL_HANDLE) {
                vkDestroyDescriptorSetLayout(vkDevice, descriptorSetLayout, nullptr);
                descriptorSetLayout = VK_NULL_HANDLE;
            }
        }

        void Pipeline::destroyOwnedVulkanObjects() {
            if (pipeline != VK_NULL_HANDLE) {
                vkDestroyPipeline(vkDevice, pipeline, nullptr);
            }
            pipeline = VK_NULL_HANDLE;

            resetBaseVulkanObjects();
        }

        void Pipeline::build() {
            resetBaseVulkanObjects();

            std::vector<VkDescriptorSetLayoutBinding> layoutBindings;
            layoutBindings.reserve(descriptorBufferResources[0].size() + descriptorTextureResources.size());

            for (const auto& bufferResource : descriptorBufferResources[0]) {
                VkDescriptorSetLayoutBinding layoutBinding{};
                layoutBinding.binding = bufferResource.binding;
                layoutBinding.descriptorType = bufferResource.type;
                layoutBinding.descriptorCount = bufferResource.descriptorCount;
                layoutBinding.stageFlags = bufferResource.stageFlags;
                layoutBinding.pImmutableSamplers = nullptr;
                layoutBindings.push_back(layoutBinding);
            }

            for (const auto& textureResource : descriptorTextureResources) {
                VkDescriptorSetLayoutBinding layoutBinding{};
                layoutBinding.binding = textureResource.binding;
                layoutBinding.descriptorType = textureResource.type;
                layoutBinding.descriptorCount = textureResource.descriptorCount;
                layoutBinding.stageFlags = textureResource.stageFlags;
                layoutBinding.pImmutableSamplers = nullptr;
                layoutBindings.push_back(layoutBinding);
            }


            VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo{};
            descriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            descriptorSetLayoutCreateInfo.bindingCount = static_cast<uint32_t>(layoutBindings.size());
            descriptorSetLayoutCreateInfo.pBindings = layoutBindings.empty() ? nullptr : layoutBindings.data();

            LCA_CHECK_VULKAN(
                vkCreateDescriptorSetLayout(vkDevice, &descriptorSetLayoutCreateInfo, nullptr, &descriptorSetLayout),
                "Pipeline::build",
                "vkCreateDescriptorSetLayout"
            )

            VkDescriptorSetAllocateInfo descriptorSetAllocateInfo{};
            descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            descriptorSetAllocateInfo.descriptorPool = vkDescriptorPool;
            descriptorSetAllocateInfo.descriptorSetCount = MAX_FRAMES_IN_FLIGHT;
            std::array<VkDescriptorSetLayout, MAX_FRAMES_IN_FLIGHT> descriptorSetLayouts{};
            descriptorSetLayouts.fill(descriptorSetLayout);
            descriptorSetAllocateInfo.pSetLayouts = descriptorSetLayouts.data();

            LCA_CHECK_VULKAN(
                vkAllocateDescriptorSets(vkDevice, &descriptorSetAllocateInfo, descriptorSets.data()),
                "Pipeline::build",
                "vkAllocateDescriptorSets"
            )

            updateDescriptorSetWrites();

            VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{};
            pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
            pipelineLayoutCreateInfo.setLayoutCount = 1;
            pipelineLayoutCreateInfo.pSetLayouts = &descriptorSetLayout;
            pipelineLayoutCreateInfo.pushConstantRangeCount = 0;
            pipelineLayoutCreateInfo.pPushConstantRanges = nullptr;

            LCA_CHECK_VULKAN(
                vkCreatePipelineLayout(vkDevice, &pipelineLayoutCreateInfo, nullptr, &pipelineLayout),
                "Pipeline::build",
                "vkCreatePipelineLayout"
            )
        }

        void Pipeline::updateDescriptorSetWrites() {
            for(uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
                updateDescriptorSetWrites(i);
            }
        }
        void Pipeline::updateDescriptorSetWrites(uint32_t frameIndex) {

            std::vector<VkDescriptorBufferInfo> bufferInfos;
            std::vector<std::vector<VkDescriptorBufferInfo>> bufferInfoArrays;
            std::vector<std::vector<VkDescriptorImageInfo>> imageInfoArrays;
            std::vector<VkWriteDescriptorSet> writes;

            bufferInfos.reserve(descriptorBufferResources[frameIndex].size());
            bufferInfoArrays.reserve(descriptorBufferResources[frameIndex].size());
            imageInfoArrays.reserve(descriptorTextureResources.size());
            writes.reserve(descriptorBufferResources[frameIndex].size() + descriptorTextureResources.size());

            for (const auto& resource : descriptorBufferResources[frameIndex]) {
                VkWriteDescriptorSet write{};
                write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                write.dstSet = descriptorSets[frameIndex];
                write.dstBinding = resource.binding;
                write.dstArrayElement = 0;
                write.descriptorType = resource.type;
                write.descriptorCount = resource.descriptorCount;

                if (resource.type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER || resource.type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER) {
                    if (resource.bufferSource != nullptr) {
                        bufferInfoArrays.emplace_back(resource.descriptorCount);
                        auto& infos = bufferInfoArrays.back();
                        const auto& sourceBuffers = *resource.bufferSource;

                        for (uint32_t i = 0; i < resource.descriptorCount; ++i) {
                            VkDescriptorBufferInfo info{};
                            if (i < sourceBuffers.size()) {
                                info.buffer = sourceBuffers[i].vkBuffer;
                            } else {
                                info.buffer = resource.fallbackBuffer;
                            }
                            LCA_ASSERT(info.buffer != VK_NULL_HANDLE, "Pipeline", "updateDescriptorSetWrites", "Buffer array entry is null.")
                            info.offset = 0;
                            info.range = VK_WHOLE_SIZE;
                            infos[i] = info;
                        }

                        write.pBufferInfo = infos.data();
                    } else if (!resource.buffers.empty()) {
                        LCA_ASSERT(
                            resource.buffers.size() == resource.descriptorCount,
                            "Pipeline",
                            "updateDescriptorSetWrites",
                            "Buffer array size must match descriptor count."
                        )

                        bufferInfoArrays.emplace_back(resource.descriptorCount);
                        auto& infos = bufferInfoArrays.back();
                        for (uint32_t i = 0; i < resource.descriptorCount; ++i) {
                            VkDescriptorBufferInfo info{};
                            info.buffer = resource.buffers[i];
                            info.offset = 0;
                            info.range = VK_WHOLE_SIZE;
                            infos[i] = info;
                        }

                        write.pBufferInfo = infos.data();
                    } else {
                        VkDescriptorBufferInfo info{};
                        info.buffer = resource.buffer;
                        info.offset = 0;
                        info.range = VK_WHOLE_SIZE;
                        bufferInfos.push_back(info);
                        write.pBufferInfo = &bufferInfos.back();
                    }

                    writes.push_back(write);
                }else{
                    LCA_LOGE("Pipeline", "updateDescriptorSetWrites", "Unsupported descriptor type in buffer resources.")
                }
            }  

            for(auto& textureResource : descriptorTextureResources) {
                VkWriteDescriptorSet write{};
                write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                write.dstSet = descriptorSets[frameIndex];
                write.dstBinding = textureResource.binding;
                write.dstArrayElement = 0;
                write.descriptorType = textureResource.type;
                write.descriptorCount = textureResource.descriptorCount;

                imageInfoArrays.emplace_back(textureResource.descriptorCount);
                auto& infos = imageInfoArrays.back();
                LCA_ASSERT(textureResource.textures != nullptr, "Pipeline", "updateDescriptorSetWrites", "Texture array pointer is null.")

                for (uint32_t i = 0; i < textureResource.descriptorCount; ++i) {
                    const Texture* selectedTexture = &(textureResource.textures[i]);

                    VkDescriptorImageInfo info{};
                    info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                    info.imageView = selectedTexture->vkImageView;
                    info.sampler = selectedTexture->vkSampler;
                    infos[i] = info;
                }

                write.pImageInfo = infos.data();
                writes.push_back(write);
            }

            if (!writes.empty()) {
                vkUpdateDescriptorSets(vkDevice, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
            }
        }

    }
}
