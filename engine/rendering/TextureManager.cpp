module;
#include <simdjson/internal/instruction_set.h>
#include <vulkan/vulkan.hpp>
#include "stb_image.h"
#include "vk_mem_alloc.h"
export module TextureManager;
import std;
import VulkanContext;
import FileLoaderSystem;
import ServiceLocator;
import Atomics;
import VulkanCommand;
#define DEFAULT_TEXTURE_SIZE 512

//TODO: have this happen async
namespace Rendering
{
    export class TextureManager : public ISystem
    {
    public:
        TextureManager() = delete;
        TextureManager(const Vulkan::Context& context)
            :m_context(context),
            current_texture_index(0),
            raw_texture_data(32, std::vector<unsigned char>(DEFAULT_TEXTURE_SIZE * DEFAULT_TEXTURE_SIZE))
        {
        }

        void loadAllPaths()
        {
            //read json for all relevant file paths
        }

        size_t loadTexture(const char* path)
        {
            //increment index and reserve the slot to prevent invalidation
            size_t index = current_texture_index++;
            raw_texture_data.emplace_back();
            {
                auto fs = ServiceLocator::Instance()->Get<AngelBase::Core::FileLoaderSystem>();
                AngelBase::Core::AsyncRequestHandle h =  fs->asyncReadFile(
                    path, 
                    raw_texture_data[index].data(), 
                    DEFAULT_TEXTURE_SIZE,
                    AngelBase::Core::AsyncFilePriority::Critical, 
                    texture_counter, 
                    nullptr);
                
                texture_metadata.push_back(h);
            }
            
            return index;
        }

        void UploadTexturesToVRAM()
        {
            texture_counter.wait_for_zero();
            
            for (size_t i = 0; i < raw_texture_data.size(); ++i)
            {
                vk::ImageCreateInfo texImgCI{};
                texImgCI.imageType = vk::ImageType::e2D;
                // .tga?
                texImgCI.format = vk::Format::eB8G8R8A8Unorm;
                // not correct we need to look at the base width and height of textures
                texImgCI.extent = vk::Extent3D{DEFAULT_TEXTURE_SIZE, DEFAULT_TEXTURE_SIZE, 1};
                // ? format dependent methinks
                texImgCI.mipLevels = 1;
                texImgCI.samples = vk::SampleCountFlagBits::e1;
                texImgCI.tiling = vk::ImageTiling::eOptimal;
                texImgCI.usage = vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled;
                texImgCI.initialLayout = vk::ImageLayout::eUndefined;

                VmaAllocationCreateInfo texImageAllocCI {};
                texImageAllocCI.usage = VMA_MEMORY_USAGE_AUTO;

                vmaCreateImage(m_context.vram_allocator,
                    reinterpret_cast<VkImageCreateInfo*>(&texImgCI), 
                    &texImageAllocCI,
                    reinterpret_cast<VkImage*>(&textures[i].image), 
                    &textures[i].allocation,
                    nullptr);

                vk::ImageViewCreateInfo texViewCI{};
                texViewCI.image = textures[i].image;
                texViewCI.viewType = vk::ImageViewType::e2D;
                texViewCI.format = texImgCI.format;
                //custom mip levels per image
                texViewCI.subresourceRange = vk::ImageSubresourceRange{vk::ImageAspectFlagBits::eColor, 0, 1};

                if (m_context.device.createImageView(&texViewCI, nullptr, &textures[i].view) != vk::Result::eSuccess)
                {
                    std::cerr << "Image view has failed to be created for uploaded texture!" << std::endl;
                }

                vk::Buffer image_source_buffer{};
                VmaAllocation image_source_allocation{};
                vk::BufferCreateInfo bufferCI{};
                //get size of each individual texture's data
                bufferCI.size = static_cast<uint32_t>(raw_texture_data[i].size());
                bufferCI.usage = vk::BufferUsageFlagBits::eTransferSrc;

                VmaAllocationCreateInfo imgSrcAllocCI{};
                imgSrcAllocCI.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
                imgSrcAllocCI.usage = VMA_MEMORY_USAGE_AUTO;

                if (vmaCreateBuffer(m_context.vram_allocator, reinterpret_cast<VkBufferCreateInfo*>(&bufferCI),
                    &imgSrcAllocCI, reinterpret_cast<VkBuffer*>(&image_source_buffer),
                    &image_source_allocation, nullptr) != VK_SUCCESS)
                {
                    std::cerr << "Failed to create image buffer!" << std::endl;
                }

                void * imgSrcBufferPtr{nullptr};
                vmaMapMemory(m_context.vram_allocator, image_source_allocation, &imgSrcBufferPtr);
                memcpy(imgSrcBufferPtr, raw_texture_data[i].data(), raw_texture_data[i].size());
            }
            
            vk::FenceCreateInfo fenceOneTimeCI {};
            vk::Fence UploadFence;
            if (!m_context.device.createFence(fenceOneTimeCI))
            {
                std::cerr << "Failed to create fence!" << std::endl;
            }
            
            vk::CommandBuffer buffer;
            {
                auto pool_manager = ServiceLocator::Instance()->Get<Vulkan::CommandPoolManager>();
                buffer = pool_manager->GetTransferCommandBuffer();
                pool_manager->BeginRecordTransferCommands();
            }
            vk::ImageMemoryBarrier2 barrier_texture_image
            {
                .srcStageMask = VK_PIPELINE_STAGE_2_NONE,
                .srcAccessMask = VK_ACCESS_2_NONE,
                .dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                .dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                .image = textures[i].image,
                .subresourceRange = { .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = ktxTexture->numLevels, .layerCount = 1 }
            };
            
            vk::DependencyInfo barrier_texture_dependency_info =
            {
                .imageMemoryBarrierCount = 1,
                .pImageMemoryBarrier = &barrier_texture_image
            };
            
            vkCmdPipelineBarrier2(buffer, barrier_texture_dependency_info);
            
            std::vector<vk::BufferImageCopy> bufferImageCopies;
            
            for (auto j = 0; j < ktxTexture->numLevels; j++) {
                ktx_size_t mipOffset{0};
                KTX_error_code ret = ktxTexture_GetImageOffset(ktxTexture, j, 0, 0, &mipOffset);
                copyRegions.push_back({
                    .bufferOffset = mipOffset,
                    .imageSubresource{.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .mipLevel = (uint32_t)j, .layerCount = 1},
                    .imageExtent{.width = ktxTexture->baseWidth >> j, .height = ktxTexture->baseHeight >> j, .depth = 1 },
                });
            }
            vkCmdCopyBufferToImage(cbOneTime, imgSrcBuffer, textures[i].image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, static_cast<uint32_t>(copyRegions.size()), copyRegions.data());
            VkImageMemoryBarrier2 barrierTexRead{
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                .srcStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT,
                .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
                .dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
                .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                .newLayout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL,
                .image = textures[i].image,
                .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = ktxTexture->numLevels, .layerCount = 1 }
            };
            barrierTexInfo.pImageMemoryBarriers = &barrierTexRead;
            vkCmdPipelineBarrier2(cbOneTime, &barrierTexInfo);
            chk(vkEndCommandBuffer(cbOneTime));
            VkSubmitInfo oneTimeSI{
                .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                .commandBufferCount = 1,
                .pCommandBuffers = &cbOneTime
            };
            chk(vkQueueSubmit(queue, 1, &oneTimeSI, fenceOneTime));
            chk(vkWaitForFences(device, 1, &fenceOneTime, VK_TRUE, UINT64_MAX));
            //next upload commands to upload all of these to GPU
            //use fences for synchronizing, and create samplers and descriptors for each texture
            
            VkSamplerCreateInfo samplerCI{
                .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
                .magFilter = VK_FILTER_LINEAR,
                .minFilter = VK_FILTER_LINEAR,
                .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
                .anisotropyEnable = VK_TRUE,
                .maxAnisotropy = 8.0f, // 8 is a widely supported value for max anisotropy
                .maxLod = (float)ktxTexture->numLevels,
            };
            chk(vkCreateSampler(device, &samplerCI, nullptr, &textures[i].sampler));
            
            ktxTexture_Destroy(ktxTexture);
            textureDescriptors.push_back({
                .sampler = textures[i].sampler,
                .imageView = textures[i].view,
                .imageLayout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL
            });
        }

        
    private:
        
        Atomics::Counter texture_counter;
        const Vulkan::Context& m_context;
        std::vector<std::vector<unsigned char>> raw_texture_data;
        std::vector<AngelBase::Core::AsyncRequestHandle> texture_metadata;
        struct VulkanImage
        {
            vk::Image image;
            vk::ImageView view;
            VmaAllocation allocation;
        };
        std::vector<VulkanImage> textures;
        size_t current_texture_index;
    };
}