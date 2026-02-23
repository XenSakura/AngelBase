module;
#include <vulkan/vulkan.hpp>
#include "stb_image.h"
#include "vk_mem_alloc.h"
export module TextureManager;
import std;
import VulkanContext;
import FileLoaderSystem;
import ServiceLocator;
import Atomics;


//TODO: have this happen async
namespace Rendering
{
    export class TextureManager
    {
    public:
        TextureManager() = delete;
        TextureManager(const Vulkan::Context& context)
            :m_context(context),
            current_texture_index(0)
        {
            raw_texture_data.reserve(1024);
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
            
            texture_counter.increment();
            Atomics::Counter local_counter = texture_counter;

            auto fs = ServiceLocator::Instance()->Get<AngelBase::Core::FileLoaderSystem>().lock().get();
            fs->submitLoadRequests({path,[local_counter, this, index](AngelBase::Core::LoadResult l) mutable
            {
                if (l.success)
                {
                    int x, y, channels;
                    unsigned char * image = stbi_load_from_memory(l.data.data(),l.data.size(), &x, &y, &channels, 4);

                    if (!image)
                        return;

                    std::vector<unsigned char> imageData(image, image + x * y * channels);
                    raw_texture_data[index] = std::move(imageData);
                    stbi_image_free(image);

                    local_counter.decrement();
                }
                else
                {
                    local_counter.decrement();
                }
            }});
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
                texImgCI.extent = vk::Extent3D{2048, 2048, 1};
                // ? format dependent methinks
                texImgCI.mipLevels = 1;
                texImgCI.samples = vk::SampleCountFlagBits::e1;
                texImgCI.tiling = vk::ImageTiling::eOptimal;
                texImgCI.usage = vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled;
                texImgCI.initialLayout = vk::ImageLayout::eUndefined;

                VmaAllocationCreateInfo texImageAllocCI {};
                texImageAllocCI.usage = VMA_MEMORY_USAGE_AUTO;

                vmaCreateImage(m_context.allocator,
                    reinterpret_cast<VkImageCreateInfo*>(&texImgCI), &texImageAllocCI,
                    reinterpret_cast<VkImage*>(&textures[i].image), &textures[i].allocation,
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

                if (vmaCreateBuffer(m_context.allocator, reinterpret_cast<VkBufferCreateInfo*>(&bufferCI),
                    &imgSrcAllocCI, reinterpret_cast<VkBuffer*>(&image_source_buffer),
                    &image_source_allocation, nullptr) != VK_SUCCESS)
                {
                    std::cerr << "Failed to create image buffer!" << std::endl;
                }

                void * imgSrcBufferPtr{nullptr};
                vmaMapMemory(m_context.allocator, image_source_allocation, &imgSrcBufferPtr);
                memcpy(imgSrcBufferPtr, raw_texture_data[i].data(), raw_texture_data[i].size());
            }
            //next upload commands to upload all of these to GPU
            //use fences for synchronizing, and create samplers and descriptors for each texture
        }

        
    private:
        
        Atomics::Counter texture_counter;
        const Vulkan::Context& m_context;
        std::vector<std::vector<unsigned char>> raw_texture_data;
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