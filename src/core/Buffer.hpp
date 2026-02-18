#pragma once

#include "Global.hpp"
#include "vma/vk_mem_alloc.h"
#include "Command.hpp"

namespace Lca
{
    namespace Core
    {

        struct Buffer
        {
            uint32_t numberElements;
            uint32_t elementSize;
            VkBuffer vkBuffer;
            VmaAllocation vmaAllocation;
            VmaAllocationInfo vmaAllocationInfo;
        };

        struct BufferInterface
        {
            uint32_t numberElements;
            uint32_t elementSize;
            VkBuffer vkBuffer;
            VmaAllocation vmaAllocation;
            VmaAllocationInfo vmaAllocationInfo;
            void* pMemory;
        };

        struct DualBuffer{
            BufferInterface interface;
            Buffer buffer;

            void recordSync(const Command& command){
                VkBufferCopy copyRegion{};
                copyRegion.size = interface.numberElements * interface.elementSize;
                vkCmdCopyBuffer(
                    command.vkCommandBuffer,
                    interface.vkBuffer,
                    buffer.vkBuffer,
                    1,
                    &copyRegion
                );
            }
        };

        struct SlotBuffer{
            uint32_t size = 0;
            std::vector<uint32_t> freeSlotIndices;
            BufferInterface interface;
            Buffer buffer;
            std::vector<uint8_t> clearValue;

            void setClearValue(const void* pData){
                clearValue.resize(interface.elementSize);
                memcpy(clearValue.data(), pData, interface.elementSize);
            }

            uint32_t add(void* pData){
                uint32_t index;
                if(freeSlotIndices.size() > 0){
                    index = freeSlotIndices.back();
                    freeSlotIndices.pop_back();
                } else {
                    LCA_ASSERT(size < interface.numberElements, "SlotBuffer", "add", "Exceeded SlotBuffer capacity.")
                    index = size;
                    size++;
                }
                memcpy(static_cast<uint8_t*>(interface.pMemory) + index * interface.elementSize, pData, interface.elementSize);
                return index;
            }

            void remove(uint32_t index, void* pData = nullptr){
                freeSlotIndices.push_back(index);

                if(pData){
                    memcpy(pData, static_cast<uint8_t*>(interface.pMemory) + index * interface.elementSize, interface.elementSize);
                }

                if (!clearValue.empty()) {
                    memcpy(static_cast<uint8_t*>(interface.pMemory) + index * interface.elementSize, clearValue.data(), interface.elementSize);
                } else {
                    memset(static_cast<uint8_t*>(interface.pMemory) + index * interface.elementSize, 0, interface.elementSize);
                }
            }

            void recordSync(const Command& command){
                LCA_ASSERT(size <= interface.numberElements, "SlotBuffer", "recordSync", "SlotBuffer size exceeds interface capacity.")
                VkBufferCopy copyRegion{};
                copyRegion.size = size * interface.elementSize;
                vkCmdCopyBuffer(
                    command.vkCommandBuffer,
                    interface.vkBuffer,
                    buffer.vkBuffer,
                    1,
                    &copyRegion
                );
            }
        };
/*        
        struct Buffer
        {
            uint32_t numberElements;
            uint32_t elementSize;
            VkBuffer vkBuffer;
            VkDeviceMemory vkDeviceMemory;
        };

        struct BufferInterface
        {
            uint32_t numberElements;
            uint32_t elementSize;
            VkBuffer vkBuffer;
            VkDeviceMemory vkDeviceMemory;
            void* pMemory;
        };
*/
        Buffer createBuffer
        (uint32_t numberElements, uint32_t elementSize,
        VkBufferUsageFlags usage);
        void destroyBuffer(Buffer buffer);

        BufferInterface createBufferInterface
        (uint32_t numberElements, uint32_t elementSize,
        VkBufferUsageFlags usage);
        void destroyBufferInterface(BufferInterface buffer);

        DualBuffer createDualBuffer
        (uint32_t numberElements, uint32_t elementSize,
        VkBufferUsageFlags usage);
        void destroyDualBuffer(DualBuffer& dualBuffer);

        SlotBuffer createSlotBuffer
        (uint32_t numberElements, uint32_t elementSize,
        VkBufferUsageFlags usage);
        void destroySlotBuffer(SlotBuffer& slotBuffer);

    }
}