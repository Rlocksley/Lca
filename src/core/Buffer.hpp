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

            void recordSync(const SingleCommand& command){
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

        struct SlotBufferGPU{
            uint32_t size = 0;
            std::vector<uint32_t> freeSlotIndices;
            BufferInterface interface;
            Buffer buffer;

            const uint32_t add(const void* data){
                uint32_t index;
                if(freeSlotIndices.size() > 0){
                    index = freeSlotIndices.back();
                    freeSlotIndices.pop_back();
                } else {
                    LCA_ASSERT(size < interface.numberElements, "SlotBufferGPU", "add", "Exceeded SlotBufferGPU capacity.")
                    index = size;
                    size++;
                }
                std::memcpy(static_cast<char*>(interface.pMemory) + index*interface.elementSize, data, interface.elementSize);
                return index;
            }

            void remove(const uint32_t index, void* pData = nullptr){
                freeSlotIndices.push_back(index);
                if (pData) {
                    std::memcpy(pData, static_cast<char*>(interface.pMemory) + index*interface.elementSize, interface.elementSize);
                }
            }

            void recordSync(const Command& command){
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


        template<typename T, uint32_t N>
        struct SlotBuffer{
        private:
            uint32_t size = 0;
            std::vector<uint32_t> freeSlotIndices;
            std::array<T, N> buffer;
            std::vector<bool> dirtyFlags;
            std::vector<uint32_t> dirtyIndices;
            std::vector<uint32_t> dirtyCopiesRemaining;

            inline void markDirty(const uint32_t index){
                LCA_ASSERT(index < buffer.size(), "SlotBuffer", "markDirty", "Index out of bounds.")
                if(!dirtyFlags[index]){
                    dirtyFlags[index] = true;
                    dirtyIndices.push_back(index);
                }
                dirtyCopiesRemaining[index] = MAX_FRAMES_IN_FLIGHT;
            }

        public:
            SlotBuffer() : dirtyFlags(N, false), dirtyCopiesRemaining(N, 0) {}

            const uint32_t getSize() const { return size; }
            const uint32_t add(const T& data){
                uint32_t index;
                if(freeSlotIndices.size() > 0){
                    index = freeSlotIndices.back();
                    freeSlotIndices.pop_back();
                } else {
                    LCA_ASSERT(size < buffer.size(), "SlotBuffer", "add", "Exceeded SlotBuffer capacity.")
                    index = size;
                    size++;
                }
                buffer[index] = data;
                markDirty(index);
                return index;
            }

            inline void update(const uint32_t index, const T& data){
                LCA_ASSERT(index < size, "SlotBuffer", "update", "Index out of bounds.")
                buffer[index] = data;
                markDirty(index);
            }

            inline void remove(const uint32_t index){
                LCA_ASSERT(index < size, "SlotBuffer", "remove", "Index out of bounds.")
                freeSlotIndices.push_back(index);

                buffer[index] = T{};
                markDirty(index);
            }

            inline void copyTo(const BufferInterface& interface){
                if(dirtyIndices.empty()){
                    return;
                }

                LCA_ASSERT(sizeof(T) <= interface.elementSize, "SlotBuffer", "copy", "Element size exceeds interface element size.")
                LCA_ASSERT(interface.numberElements >= buffer.size(), "SlotBuffer", "copy", "Interface capacity is smaller than SlotBuffer capacity.")

                std::vector<uint32_t> remainingDirtyIndices;
                remainingDirtyIndices.reserve(dirtyIndices.size());

                for(uint32_t i = 0; i < dirtyIndices.size(); i++){
                    const uint32_t index = dirtyIndices[i];

                    std::memcpy(
                        static_cast<char*>(interface.pMemory) + (static_cast<size_t>(index) * interface.elementSize),
                        &buffer[index],
                        sizeof(T)
                    );

                    if(dirtyCopiesRemaining[index] > 0){
                        dirtyCopiesRemaining[index]--;
                    }

                    if(dirtyCopiesRemaining[index] == 0){
                        dirtyFlags[index] = false;
                    } else {
                        remainingDirtyIndices.push_back(index);
                    }
                }

                dirtyIndices.swap(remainingDirtyIndices);
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

        SlotBufferGPU createSlotBufferGPU
        (uint32_t numberElements, uint32_t elementSize,
        VkBufferUsageFlags usage);
        void destroySlotBufferGPU(SlotBufferGPU& slotBufferGPU);

    }
}