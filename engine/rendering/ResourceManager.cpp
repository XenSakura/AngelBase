module;
#include "vulkan/vulkan.hpp"
#include "assimp/Vertex.h"
#include "vma/vk_mem_alloc.h"

export module ResourceManager;

import VulkanContext;

export using glTFVertex = Assimp::Vertex;

namespace Rendering::Vulkan
{
    //temp
    Context m_context;
    
    export class ResourceManager
    {
        
        std::weak_ptr<std::vector<glTFVertex>> model_vertices_data;
        std::weak_ptr<std::vector<uint16_t>> model_indices_data;
        
        void UploadModelToGPU()
        {
            //for interfacing with the model
            VkDeviceSize vBufSize{sizeof(glTFVertex) * model_vertices_data.lock().get()->size()};
            VkDeviceSize iBufSize{sizeof(uint16_t) * model_indices_data.lock().get()->size()};
            //aiVector3D min, max;

            vk::BufferCreateInfo buffer_create_info{};
            buffer_create_info.size = vBufSize + iBufSize;
            buffer_create_info.usage = vk::BufferUsageFlags::BitsType::eVertexBuffer | vk::BufferUsageFlags::BitsType::eIndexBuffer;

            VmaAllocationCreateInfo bufferAllocInfo {};
            bufferAllocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT;
            bufferAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;

            vk::Buffer vBuffer{};
            VmaAllocation vBufferAllocation {};
            //buffer for uploading model textures to VRAM
            vmaCreateBuffer(m_context.allocator, 
                reinterpret_cast<VkBufferCreateInfo*>(&buffer_create_info), 
                &bufferAllocInfo, 
                reinterpret_cast<VkBuffer*>(&vBuffer), 
                &vBufferAllocation, nullptr);

            void * bufferPtr {nullptr};

            vmaMapMemory(m_context.allocator, vBufferAllocation, &bufferPtr);
            memcpy(bufferPtr, model_vertices_data.lock().get()->data(), vBufSize);
            memcpy(static_cast<char*>(bufferPtr) + vBufSize, model_indices_data.lock().get()->data(), iBufSize );
            vmaUnmapMemory(m_context.allocator, vBufferAllocation);
        }
    };
}