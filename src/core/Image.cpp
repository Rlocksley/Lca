#include "Image.hpp"
#include "PhysicalDevice.hpp"
#include "Device.hpp"
#include "Allocator.hpp"
#include "Command.hpp"
#include "Mutex.hpp"
#include "Buffer.hpp"
#include "Queue.hpp"
#include <fstream>
#include <vector>
#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"
#include "Renderer.hpp"
namespace Lca
{
    namespace Core
    {

        Image createImage
        (uint32_t width, uint32_t height,
        VkFormat format,
        VkImageUsageFlags usage,
        VkSampleCountFlagBits samples,
        VkImageAspectFlags aspect)
        {
            Image image;
            image.width = width;
            image.height = height;

            VkImageCreateInfo createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
            createInfo.imageType = VK_IMAGE_TYPE_2D;
            createInfo.extent.width = width;
            createInfo.extent.height = height;
            createInfo.extent.depth = 1;
            createInfo.mipLevels = 1;
            createInfo.arrayLayers = 1;
            createInfo.format = format;
            createInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            createInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
            createInfo.usage = usage;
            createInfo.samples = samples;

            VmaAllocationCreateInfo allocInfo{};
            allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
            allocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
            allocInfo.priority = 1.f;

            LCA_CHECK_VULKAN
            (vmaCreateImage
            (vmaAllocator,
            &createInfo,
            &allocInfo,
            &image.vkImage,
            &image.vmaAllocation,
            nullptr),
            "createImage",
            "vmaCreateImage")

            VkImageViewCreateInfo ivCreateInfo{};
            ivCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            ivCreateInfo.image = image.vkImage;
            ivCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            ivCreateInfo.format = format;
            ivCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            ivCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            ivCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            ivCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
            ivCreateInfo.subresourceRange.aspectMask = aspect;
            ivCreateInfo.subresourceRange.baseMipLevel = 0;
            ivCreateInfo.subresourceRange.levelCount = 1;
            ivCreateInfo.subresourceRange.baseArrayLayer = 0;
            ivCreateInfo.subresourceRange.layerCount = 1;

            LCA_CHECK_VULKAN
            (vkCreateImageView
            (vkDevice,
            &ivCreateInfo,
            nullptr,
            &image.vkImageView),
            "createImage",
            "vkCreateImageView")

            return image;
        }

        void destroyImage(Image image)
        {

            vkDestroyImageView
            (vkDevice,
            image.vkImageView,
            nullptr);

            vmaDestroyImage
            (vmaAllocator,
            image.vkImage,
            image.vmaAllocation);
        }

        Texture createTexture(uint32_t width, uint32_t height, void* pixels){
            Texture image;
            image.width = width;
            image.height = height;

            VkImageCreateInfo createInfo;
            createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
            createInfo.flags = 0;
            createInfo.imageType = VK_IMAGE_TYPE_2D;
            createInfo.extent.width = width;
            createInfo.extent.height = height;
            createInfo.extent.depth = 1;
            createInfo.mipLevels = 1;
            createInfo.arrayLayers = 1;
            createInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
            createInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
            createInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            createInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
            createInfo.samples = VK_SAMPLE_COUNT_1_BIT;
            createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            createInfo.queueFamilyIndexCount = 0;
            createInfo.pQueueFamilyIndices = NULL;
            createInfo.pNext = NULL;

            VmaAllocationCreateInfo allocInfo{};
            allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
            allocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
            allocInfo.priority = 1.f;

            LCA_VK_MUTEX(
            LCA_CHECK_VULKAN
            (vmaCreateImage
            (vmaAllocator,
            &createInfo,
            &allocInfo,
            &image.vkImage,
            &image.vmaAllocation,
            nullptr),
            "createImage",
            "vmaCreateImage"))

            VkImageViewCreateInfo ivCreateInfo;
            ivCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            ivCreateInfo.flags = 0;
            ivCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            ivCreateInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
            ivCreateInfo.image = image.vkImage;
            ivCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            ivCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            ivCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            ivCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
            ivCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            ivCreateInfo.subresourceRange.baseMipLevel = 0;
            ivCreateInfo.subresourceRange.levelCount = 1;
            ivCreateInfo.subresourceRange.baseArrayLayer = 0;
            ivCreateInfo.subresourceRange.layerCount = 1;
            ivCreateInfo.pNext = NULL;

            LCA_VK_MUTEX(
            LCA_CHECK_VULKAN
            (vkCreateImageView
            (vkDevice,
            &ivCreateInfo, NULL,
            &image.vkImageView),
            "createImage",
            "vkCreateImageView"))
            
            VkSamplerCreateInfo samplerCreateInfo;
            samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
            samplerCreateInfo.flags = 0;
            samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
            samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
            samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            samplerCreateInfo.anisotropyEnable = VK_TRUE;
            samplerCreateInfo.maxAnisotropy = vkPhysicalDeviceProperties.limits.maxSamplerAnisotropy;
            samplerCreateInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
            samplerCreateInfo.unnormalizedCoordinates = VK_FALSE;
            samplerCreateInfo.compareEnable = VK_FALSE;
            samplerCreateInfo.compareOp = VK_COMPARE_OP_ALWAYS;
            samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
            samplerCreateInfo.mipLodBias = 0.f;
            samplerCreateInfo.minLod = 0.f;
            samplerCreateInfo.maxLod = 0.f;
            samplerCreateInfo.pNext = NULL;

            LCA_VK_MUTEX(
            LCA_CHECK_VULKAN
            (vkCreateSampler
            (vkDevice,
            &samplerCreateInfo, 
            NULL,
            &image.vkSampler),
            "createImage",
            "vkCreateSampler"))

            BufferInterface copyBuffer =
            createBufferInterface
            (width * height, 4, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
            memcpy(copyBuffer.pMemory, pixels, width * height * 4);

            VkImageMemoryBarrier barrier0;
            barrier0.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barrier0.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            barrier0.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            barrier0.srcAccessMask = 0;
            barrier0.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier0.image = image.vkImage;
            barrier0.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier0.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier0.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            barrier0.subresourceRange.baseMipLevel = 0;
            barrier0.subresourceRange.levelCount = 1;
            barrier0.subresourceRange.baseArrayLayer = 0;
            barrier0.subresourceRange.layerCount = 1;
            barrier0.pNext = NULL;

            VkBufferImageCopy copy;
            copy.bufferOffset = 0;
            copy.bufferRowLength = 0;
            copy.bufferImageHeight = 0;
            copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            copy.imageSubresource.mipLevel = 0;
            copy.imageSubresource.baseArrayLayer = 0;
            copy.imageSubresource.layerCount = 1;
            copy.imageOffset.x = 0;
            copy.imageOffset.y = 0;
            copy.imageOffset.z = 0;
            copy.imageExtent.width = width;
            copy.imageExtent.height = height;
            copy.imageExtent.depth = 1;

            VkImageMemoryBarrier barrier1;
            barrier1.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barrier1.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            barrier1.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            barrier1.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier1.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            barrier1.image = image.vkImage;
            barrier1.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier1.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier1.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            barrier1.subresourceRange.baseMipLevel = 0;
            barrier1.subresourceRange.levelCount = 1;
            barrier1.subresourceRange.baseArrayLayer = 0;
            barrier1.subresourceRange.layerCount = 1;
            barrier1.pNext = NULL;

            LCA_VK_MUTEX(
            beginSingleCommand(singleCommand);

            vkCmdPipelineBarrier
            (singleCommand.vkCommandBuffer,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            0,
            0, NULL,
            0, NULL,
            1, &barrier0);

            vkCmdCopyBufferToImage
            (singleCommand.vkCommandBuffer,
            copyBuffer.vkBuffer,
            image.vkImage,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1, &copy);

            vkCmdPipelineBarrier
            (singleCommand.vkCommandBuffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            0,
            0, NULL,
            0, NULL,
            1, &barrier1);

            endSingleCommand(singleCommand);
            submitSingleCommand(singleCommand);)

            destroyBufferInterface(copyBuffer);

            return image;

        }

        void destroyTexture(Texture& texture){
            LCA_VK_MUTEX(
            vkDestroySampler(vkDevice, texture.vkSampler, nullptr);
            vkDestroyImageView(vkDevice, texture.vkImageView, nullptr);
            vmaDestroyImage(vmaAllocator, texture.vkImage, texture.vmaAllocation);)
        }

        // Creates a texture with a full mip-chain generated on the GPU via
        // vkCmdBlitImage (LINEAR filter).  The sampler uses maxLod = full chain
        // so the driver picks the best LOD automatically, giving correct
        // filtering even when a large texture is displayed in a small rect.
        Texture createTextureMipmapped(uint32_t width, uint32_t height, void* pixels) {
            Texture image;
            image.width  = width;
            image.height = height;

            const uint32_t mipLevels = static_cast<uint32_t>(
                std::floor(std::log2(static_cast<double>(std::max(width, height))))) + 1u;

            // ── Image allocation ─────────────────────────────────────────────
            // TRANSFER_SRC_BIT is required so each mip level can be blitted FROM
            VkImageCreateInfo createInfo{};
            createInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
            createInfo.imageType     = VK_IMAGE_TYPE_2D;
            createInfo.extent        = { width, height, 1 };
            createInfo.mipLevels     = mipLevels;
            createInfo.arrayLayers   = 1;
            createInfo.format        = VK_FORMAT_R8G8B8A8_SRGB;
            createInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
            createInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            createInfo.usage         = VK_IMAGE_USAGE_TRANSFER_SRC_BIT
                                     | VK_IMAGE_USAGE_TRANSFER_DST_BIT
                                     | VK_IMAGE_USAGE_SAMPLED_BIT;
            createInfo.samples       = VK_SAMPLE_COUNT_1_BIT;
            createInfo.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;

            VmaAllocationCreateInfo allocInfo{};
            allocInfo.usage    = VMA_MEMORY_USAGE_AUTO;
            allocInfo.flags    = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
            allocInfo.priority = 1.f;

            LCA_VK_MUTEX(
            LCA_CHECK_VULKAN(
                vmaCreateImage(vmaAllocator, &createInfo, &allocInfo,
                               &image.vkImage, &image.vmaAllocation, nullptr),
                "createTextureMipmapped", "vmaCreateImage"))

            // ── Image view over ALL mip levels ───────────────────────────────
            VkImageViewCreateInfo ivCI{};
            ivCI.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            ivCI.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
            ivCI.format                          = VK_FORMAT_R8G8B8A8_SRGB;
            ivCI.image                           = image.vkImage;
            ivCI.components.r                    = VK_COMPONENT_SWIZZLE_IDENTITY;
            ivCI.components.g                    = VK_COMPONENT_SWIZZLE_IDENTITY;
            ivCI.components.b                    = VK_COMPONENT_SWIZZLE_IDENTITY;
            ivCI.components.a                    = VK_COMPONENT_SWIZZLE_IDENTITY;
            ivCI.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
            ivCI.subresourceRange.baseMipLevel   = 0;
            ivCI.subresourceRange.levelCount     = mipLevels;
            ivCI.subresourceRange.baseArrayLayer = 0;
            ivCI.subresourceRange.layerCount     = 1;

            LCA_VK_MUTEX(
            LCA_CHECK_VULKAN(
                vkCreateImageView(vkDevice, &ivCI, nullptr, &image.vkImageView),
                "createTextureMipmapped", "vkCreateImageView"))

            // ── Sampler: uses the full mip-chain ─────────────────────────────
            VkSamplerCreateInfo sampCI{};
            sampCI.sType            = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
            sampCI.minFilter        = VK_FILTER_LINEAR;
            sampCI.magFilter        = VK_FILTER_LINEAR;
            sampCI.addressModeU     = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            sampCI.addressModeV     = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            sampCI.addressModeW     = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            sampCI.anisotropyEnable = VK_TRUE;
            sampCI.maxAnisotropy    = vkPhysicalDeviceProperties.limits.maxSamplerAnisotropy;
            sampCI.borderColor      = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
            sampCI.mipmapMode       = VK_SAMPLER_MIPMAP_MODE_LINEAR;
            sampCI.mipLodBias       = 0.f;
            sampCI.minLod           = 0.f;
            sampCI.maxLod           = static_cast<float>(mipLevels); // ← key fix

            LCA_VK_MUTEX(
            LCA_CHECK_VULKAN(
                vkCreateSampler(vkDevice, &sampCI, nullptr, &image.vkSampler),
                "createTextureMipmapped", "vkCreateSampler"))

            // ── Staging buffer ────────────────────────────────────────────────
            BufferInterface copyBuffer =
                createBufferInterface(width * height, 4, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
            memcpy(copyBuffer.pMemory, pixels, static_cast<size_t>(width) * height * 4);

            // ── GPU commands: upload mip 0, then blit down the chain ─────────
            // Use lock_guard directly — struct initializer-list commas inside {}
            // are not protected in the LCA_VK_MUTEX macro expansion.
            {
                std::lock_guard<std::recursive_mutex> guard(vulkanCommandMutex);
                beginSingleCommand(singleCommand);
                VkCommandBuffer cmd = singleCommand.vkCommandBuffer;

                // Transition ALL mip levels UNDEFINED → TRANSFER_DST
                VkImageMemoryBarrier allDst{};
                allDst.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                allDst.oldLayout                       = VK_IMAGE_LAYOUT_UNDEFINED;
                allDst.newLayout                       = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                allDst.srcAccessMask                   = 0;
                allDst.dstAccessMask                   = VK_ACCESS_TRANSFER_WRITE_BIT;
                allDst.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
                allDst.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
                allDst.image                           = image.vkImage;
                allDst.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
                allDst.subresourceRange.baseMipLevel   = 0;
                allDst.subresourceRange.levelCount     = mipLevels;
                allDst.subresourceRange.baseArrayLayer = 0;
                allDst.subresourceRange.layerCount     = 1;
                vkCmdPipelineBarrier(cmd,
                    VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                    VK_PIPELINE_STAGE_TRANSFER_BIT,
                    0, 0, nullptr, 0, nullptr, 1, &allDst);

                // Copy staging buffer → mip level 0
                VkBufferImageCopy region{};
                region.imageSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
                region.imageSubresource.mipLevel       = 0;
                region.imageSubresource.baseArrayLayer = 0;
                region.imageSubresource.layerCount     = 1;
                region.imageExtent                     = { width, height, 1 };
                vkCmdCopyBufferToImage(cmd, copyBuffer.vkBuffer, image.vkImage,
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

                // Generate mip levels 1..N-1 via blits
                int32_t mipW = static_cast<int32_t>(width);
                int32_t mipH = static_cast<int32_t>(height);

                for (uint32_t i = 1; i < mipLevels; ++i) {
                    // Transition previous level: DST → SRC (ready to blit from)
                    VkImageMemoryBarrier toSrc{};
                    toSrc.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                    toSrc.oldLayout                       = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                    toSrc.newLayout                       = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
                    toSrc.srcAccessMask                   = VK_ACCESS_TRANSFER_WRITE_BIT;
                    toSrc.dstAccessMask                   = VK_ACCESS_TRANSFER_READ_BIT;
                    toSrc.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
                    toSrc.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
                    toSrc.image                           = image.vkImage;
                    toSrc.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
                    toSrc.subresourceRange.baseMipLevel   = i - 1;
                    toSrc.subresourceRange.levelCount     = 1;
                    toSrc.subresourceRange.baseArrayLayer = 0;
                    toSrc.subresourceRange.layerCount     = 1;
                    vkCmdPipelineBarrier(cmd,
                        VK_PIPELINE_STAGE_TRANSFER_BIT,
                        VK_PIPELINE_STAGE_TRANSFER_BIT,
                        0, 0, nullptr, 0, nullptr, 1, &toSrc);

                    // Blit from level i-1 → level i (half resolution)
                    int32_t nextW = mipW > 1 ? mipW / 2 : 1;
                    int32_t nextH = mipH > 1 ? mipH / 2 : 1;

                    VkImageBlit blit{};
                    blit.srcSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
                    blit.srcSubresource.mipLevel       = i - 1;
                    blit.srcSubresource.baseArrayLayer = 0;
                    blit.srcSubresource.layerCount     = 1;
                    blit.srcOffsets[0]                 = { 0, 0, 0 };
                    blit.srcOffsets[1]                 = { mipW, mipH, 1 };
                    blit.dstSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
                    blit.dstSubresource.mipLevel       = i;
                    blit.dstSubresource.baseArrayLayer = 0;
                    blit.dstSubresource.layerCount     = 1;
                    blit.dstOffsets[0]                 = { 0, 0, 0 };
                    blit.dstOffsets[1]                 = { nextW, nextH, 1 };
                    vkCmdBlitImage(cmd,
                        image.vkImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                        image.vkImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                        1, &blit, VK_FILTER_LINEAR);

                    // Transition level i-1 → SHADER_READ_ONLY — done with it
                    VkImageMemoryBarrier toRead{};
                    toRead.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                    toRead.oldLayout                       = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
                    toRead.newLayout                       = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                    toRead.srcAccessMask                   = VK_ACCESS_TRANSFER_READ_BIT;
                    toRead.dstAccessMask                   = VK_ACCESS_SHADER_READ_BIT;
                    toRead.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
                    toRead.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
                    toRead.image                           = image.vkImage;
                    toRead.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
                    toRead.subresourceRange.baseMipLevel   = i - 1;
                    toRead.subresourceRange.levelCount     = 1;
                    toRead.subresourceRange.baseArrayLayer = 0;
                    toRead.subresourceRange.layerCount     = 1;
                    vkCmdPipelineBarrier(cmd,
                        VK_PIPELINE_STAGE_TRANSFER_BIT,
                        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                        0, 0, nullptr, 0, nullptr, 1, &toRead);

                    mipW = nextW;
                    mipH = nextH;
                }

                // Transition the last mip level DST → SHADER_READ_ONLY
                VkImageMemoryBarrier lastMip{};
                lastMip.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                lastMip.oldLayout                       = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                lastMip.newLayout                       = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                lastMip.srcAccessMask                   = VK_ACCESS_TRANSFER_WRITE_BIT;
                lastMip.dstAccessMask                   = VK_ACCESS_SHADER_READ_BIT;
                lastMip.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
                lastMip.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
                lastMip.image                           = image.vkImage;
                lastMip.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
                lastMip.subresourceRange.baseMipLevel   = mipLevels - 1;
                lastMip.subresourceRange.levelCount     = 1;
                lastMip.subresourceRange.baseArrayLayer = 0;
                lastMip.subresourceRange.layerCount     = 1;
                vkCmdPipelineBarrier(cmd,
                    VK_PIPELINE_STAGE_TRANSFER_BIT,
                    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                    0, 0, nullptr, 0, nullptr, 1, &lastMip);

                endSingleCommand(singleCommand);
                submitSingleCommand(singleCommand);
            }

            destroyBufferInterface(copyBuffer);
            return image;
        }

        Texture createFontTexture(uint32_t width, uint32_t height, void* pixels){
            Texture image;
            image.width = width;
            image.height = height;

            VkImageCreateInfo createInfo;
            createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
            createInfo.flags = 0;
            createInfo.imageType = VK_IMAGE_TYPE_2D;
            createInfo.extent.width = width;
            createInfo.extent.height = height;
            createInfo.extent.depth = 1;
            createInfo.mipLevels = 1;
            createInfo.arrayLayers = 1;
            createInfo.format = VK_FORMAT_R8_UNORM;
            createInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
            createInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            createInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
            createInfo.samples = VK_SAMPLE_COUNT_1_BIT;
            createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            createInfo.queueFamilyIndexCount = 0;
            createInfo.pQueueFamilyIndices = NULL;
            createInfo.pNext = NULL;

            VmaAllocationCreateInfo allocInfo{};
            allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
            allocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
            allocInfo.priority = 1.f;

            LCA_VK_MUTEX(
            LCA_CHECK_VULKAN
            (vmaCreateImage
            (vmaAllocator,
            &createInfo,
            &allocInfo,
            &image.vkImage,
            &image.vmaAllocation,
            nullptr),
            "createFontTexture",
            "vmaCreateImage"))

            VkImageViewCreateInfo ivCreateInfo;
            ivCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            ivCreateInfo.flags = 0;
            ivCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            ivCreateInfo.format = VK_FORMAT_R8_UNORM;
            ivCreateInfo.image = image.vkImage;
            ivCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            ivCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            ivCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            ivCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
            ivCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            ivCreateInfo.subresourceRange.baseMipLevel = 0;
            ivCreateInfo.subresourceRange.levelCount = 1;
            ivCreateInfo.subresourceRange.baseArrayLayer = 0;
            ivCreateInfo.subresourceRange.layerCount = 1;
            ivCreateInfo.pNext = NULL;

            LCA_VK_MUTEX(
            LCA_CHECK_VULKAN
            (vkCreateImageView
            (vkDevice,
            &ivCreateInfo, NULL,
            &image.vkImageView),
            "createFontTexture",
            "vkCreateImageView"))
            
            VkSamplerCreateInfo samplerCreateInfo;
            samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
            samplerCreateInfo.flags = 0;
            samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
            samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
            samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            samplerCreateInfo.anisotropyEnable = VK_FALSE;
            samplerCreateInfo.maxAnisotropy = 1.0f;
            samplerCreateInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
            samplerCreateInfo.unnormalizedCoordinates = VK_FALSE;
            samplerCreateInfo.compareEnable = VK_FALSE;
            samplerCreateInfo.compareOp = VK_COMPARE_OP_ALWAYS;
            samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
            samplerCreateInfo.mipLodBias = 0.f;
            samplerCreateInfo.minLod = 0.f;
            samplerCreateInfo.maxLod = 0.f;
            samplerCreateInfo.pNext = NULL;

            LCA_VK_MUTEX(
            LCA_CHECK_VULKAN
            (vkCreateSampler
            (vkDevice,
            &samplerCreateInfo, 
            NULL,
            &image.vkSampler),
            "createFontTexture",
            "vkCreateSampler"))

            BufferInterface copyBuffer =
            createBufferInterface
            (width * height, 1, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
            memcpy(copyBuffer.pMemory, pixels, width * height);

            VkImageMemoryBarrier barrier0;
            barrier0.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barrier0.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            barrier0.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            barrier0.srcAccessMask = 0;
            barrier0.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier0.image = image.vkImage;
            barrier0.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier0.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier0.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            barrier0.subresourceRange.baseMipLevel = 0;
            barrier0.subresourceRange.levelCount = 1;
            barrier0.subresourceRange.baseArrayLayer = 0;
            barrier0.subresourceRange.layerCount = 1;
            barrier0.pNext = NULL;

            VkBufferImageCopy copy;
            copy.bufferOffset = 0;
            copy.bufferRowLength = 0;
            copy.bufferImageHeight = 0;
            copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            copy.imageSubresource.mipLevel = 0;
            copy.imageSubresource.baseArrayLayer = 0;
            copy.imageSubresource.layerCount = 1;
            copy.imageOffset.x = 0;
            copy.imageOffset.y = 0;
            copy.imageOffset.z = 0;
            copy.imageExtent.width = width;
            copy.imageExtent.height = height;
            copy.imageExtent.depth = 1;

            VkImageMemoryBarrier barrier1;
            barrier1.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barrier1.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            barrier1.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            barrier1.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier1.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            barrier1.image = image.vkImage;
            barrier1.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier1.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier1.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            barrier1.subresourceRange.baseMipLevel = 0;
            barrier1.subresourceRange.levelCount = 1;
            barrier1.subresourceRange.baseArrayLayer = 0;
            barrier1.subresourceRange.layerCount = 1;
            barrier1.pNext = NULL;

            LCA_VK_MUTEX(
            beginSingleCommand(singleCommand);

            vkCmdPipelineBarrier
            (singleCommand.vkCommandBuffer,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            0,
            0, NULL,
            0, NULL,
            1, &barrier0);

            vkCmdCopyBufferToImage
            (singleCommand.vkCommandBuffer,
            copyBuffer.vkBuffer,
            image.vkImage,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1, &copy);

            vkCmdPipelineBarrier
            (singleCommand.vkCommandBuffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            0,
            0, NULL,
            0, NULL,
            1, &barrier1);

            endSingleCommand(singleCommand);
            submitSingleCommand(singleCommand);)

            destroyBufferInterface(copyBuffer);

            return image;
        }

        

        void createShadowMapArray(uint32_t width, uint32_t height, uint32_t count, VkFormat format){
            shadowMapArray.width = width;
            shadowMapArray.height = height;
            shadowMapArray.count = count;
            shadowMapArray.format = format;

            // Create the Vulkan image
            VkImageCreateInfo createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
            createInfo.imageType = VK_IMAGE_TYPE_2D;
            createInfo.extent.width = width;
            createInfo.extent.height = height;
            createInfo.extent.depth = 1;
            createInfo.mipLevels = 1;
            createInfo.arrayLayers = count;
            createInfo.format = format;
            createInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            createInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
            createInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
            createInfo.samples = VK_SAMPLE_COUNT_1_BIT;

            VmaAllocationCreateInfo allocInfo{};
            allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
            allocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
            allocInfo.priority = 1.f;

            LCA_CHECK_VULKAN
            (vmaCreateImage
            (vmaAllocator,
            &createInfo,
            &allocInfo,
            &shadowMapArray.vkImage,
            &shadowMapArray.vmaAllocation,
            &shadowMapArray.vmaAllocationInfo),
            "createShadowMapArray",
            "vkCreateImage")

           

            VkImageViewCreateInfo viewInfo{};
            viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            viewInfo.image = shadowMapArray.vkImage;
            viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
            viewInfo.format = format;
            viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
            viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
            viewInfo.subresourceRange.baseMipLevel = 0;
            viewInfo.subresourceRange.levelCount = 1;
            viewInfo.subresourceRange.baseArrayLayer = 0;
            viewInfo.subresourceRange.layerCount = count;

            LCA_CHECK_VULKAN
            (vkCreateImageView(vkDevice, &viewInfo, nullptr, &shadowMapArray.vkImageView),
            "createShadowMapArray",
            "vkCreateImageView")

            VkSamplerCreateInfo samplerInfo{};
            samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
            samplerInfo.magFilter = VK_FILTER_NEAREST;
            samplerInfo.minFilter = VK_FILTER_NEAREST;
            samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
            samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
            samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
            samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
            samplerInfo.unnormalizedCoordinates = VK_FALSE;
            samplerInfo.compareEnable = VK_TRUE;
            samplerInfo.compareOp = VK_COMPARE_OP_LESS;
            samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;

            LCA_CHECK_VULKAN
            (vkCreateSampler(vkDevice, &samplerInfo, nullptr, &shadowMapArray.vkSampler),
            "createShadowMapArray",
            "vkCreateSampler")
        }

        void destroyShadowMapArray(){
            vkDestroySampler(vkDevice, shadowMapArray.vkSampler, nullptr);
            vkDestroyImageView(vkDevice, shadowMapArray.vkImageView, nullptr);
            vmaDestroyImage(vmaAllocator, shadowMapArray.vkImage, shadowMapArray.vmaAllocation);
        }
        /*  

        Image createImage
        (uint32_t width, uint32_t height,
        VkFormat format,
        VkImageUsageFlags usage,
        VkSampleCountFlagBits samples,
        VkImageAspectFlags aspect)
        {
            Image image;
            image.width = width;
            image.height = height;

            VkImageCreateInfo createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
            createInfo.imageType = VK_IMAGE_TYPE_2D;
            createInfo.extent.width = width;
            createInfo.extent.height = height;
            createInfo.extent.depth = 1;
            createInfo.mipLevels = 1;
            createInfo.arrayLayers = 1;
            createInfo.format = format;
            createInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            createInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
            createInfo.usage = usage;
            createInfo.samples = samples;

            LCA_CHECK_VULKAN
            (vkCreateImage
            (vkDevice,
            &createInfo,
            nullptr,
            &image.vkImage),
            "createImage",
            "vkCreateImage")

            VkMemoryRequirements memRequ;
            vkGetImageMemoryRequirements
            (vkDevice,
            image.vkImage,
            &memRequ);

            VkMemoryAllocateInfo allocInfo{};
            allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
            allocInfo.allocationSize = memRequ.size;
            allocInfo.memoryTypeIndex = 
            findMemoryType
            (memRequ.memoryTypeBits,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            vkPhysicalDevice);

            LCA_CHECK_VULKAN
            (vkAllocateMemory
            (vkDevice,
            &allocInfo,
            nullptr,
            &image.vkDeviceMemory),
            "createImage",
            "vkAllocateMemory")

            LCA_CHECK_VULKAN
            (vkBindImageMemory
            (vkDevice,
            image.vkImage,
            image.vkDeviceMemory,
            0),
            "createImage",
            "vkBindImageMemory")

            VkImageViewCreateInfo ivCreateInfo{};
            ivCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            ivCreateInfo.image = image.vkImage;
            ivCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            ivCreateInfo.format = format;
            ivCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            ivCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            ivCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            ivCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
            ivCreateInfo.subresourceRange.aspectMask = aspect;
            ivCreateInfo.subresourceRange.baseMipLevel = 0;
            ivCreateInfo.subresourceRange.levelCount = 1;
            ivCreateInfo.subresourceRange.baseArrayLayer = 0;
            ivCreateInfo.subresourceRange.layerCount = 1;

            LCA_CHECK_VULKAN
            (vkCreateImageView
            (vkDevice,
            &ivCreateInfo,
            nullptr,
            &image.vkImageView),
            "createImage",
            "vkCreateImageView")

            return image;
        }

        void destroyImage(Image image)
        {
            vkDestroyImageView
            (vkDevice,
            image.vkImageView,
            nullptr);

            vkFreeMemory
            (vkDevice,
            image.vkDeviceMemory,
            nullptr);

            vkDestroyImage
            (vkDevice,
            image.vkImage,
            nullptr);
        }
*/

        Core::TextureCPU loadTexture(const std::string& filePath) {
            // Load the texture from the specified file path
            Core::TextureCPU texture;
            int width, height, numberChannels;
            texture.pixels = stbi_load(filePath.c_str(), &width, &height, &numberChannels, STBI_rgb_alpha);
            LCA_ASSERT(texture.pixels, "Asset", "loadTexture", ("Failed to load texture from file: " + filePath).c_str());
            texture.width = width;
            texture.height = height;
            return texture;
        }
 
        void freeTexture(Core::TextureCPU& texture){
            stbi_image_free(texture.pixels);
            texture.pixels = nullptr;
            texture.width = 0;
            texture.height = 0;
        }

        Texture createDepthMap(uint32_t width, uint32_t height){
            // Create a depth image with format from PhysicalDevice
            Image depthImage = createImage(width, height, depthVkFormat,
                VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                VK_SAMPLE_COUNT_1_BIT,
                VK_IMAGE_ASPECT_DEPTH_BIT);

            Texture tex{};
            tex.width = depthImage.width;
            tex.height = depthImage.height;
            tex.vkImage = depthImage.vkImage;
            tex.vmaAllocation = depthImage.vmaAllocation;
            tex.vkImageView = depthImage.vkImageView;

            VkSamplerCreateInfo samplerInfo{};
            samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
            samplerInfo.magFilter = VK_FILTER_NEAREST;
            samplerInfo.minFilter = VK_FILTER_NEAREST;
            samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            samplerInfo.anisotropyEnable = VK_FALSE;
            samplerInfo.maxAnisotropy = 1.0f;
            samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
            samplerInfo.unnormalizedCoordinates = VK_FALSE;
            samplerInfo.compareEnable = VK_FALSE;
            samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
            samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;

            LCA_CHECK_VULKAN(
                vkCreateSampler(vkDevice, &samplerInfo, nullptr, &tex.vkSampler),
                "createDepthMap",
                "vkCreateSampler"
            )

            return tex;
        }

    }
}