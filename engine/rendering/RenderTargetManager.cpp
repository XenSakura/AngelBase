module;
#include "vulkan/vulkan.hpp"

#include "vk_mem_alloc.h"

export module RenderTargetManager;

import VulkanContext;
import ServiceLocator;
namespace Rendering::Vulkan
{
    

    export typedef Rendering::Vulkan::Image RenderTarget;
    
    export class RenderTargetManager : public ISystem
    {
    private:
        const Vulkan::Context& context;
        RenderTarget draw_color_target;
        RenderTarget draw_depth_target;
        /**
         * Creates a render target, allocates the memory for the image, and creates the image view
         * @param test 
         * @param format 
         * @param size 
         * @param usage 
         * @return 
         */
        RenderTarget createRenderTarget(const vk::ImageAspectFlags::BitsType& test, const vk::Format& format, const vk::Extent3D& size, const vk::ImageUsageFlags& usage) const
        {
            
            vk::ImageCreateInfo image_info = {};
            image_info.usage = usage;
            image_info.format = format;
            image_info.extent = VkExtent3D{size.width, size.height, 1};
            //2d image for rendering to screen
            image_info.imageType = vk::ImageType::e2D;
            image_info.mipLevels = 1;
            image_info.arrayLayers = 1;
            // for MSAA
            image_info.samples = vk::SampleCountFlagBits::e1;
            // optimal tiling for best gpu format
            image_info.tiling = vk::ImageTiling::eOptimal;
            image_info.sharingMode = vk::SharingMode::eExclusive;
            image_info.initialLayout = vk::ImageLayout::eUndefined;

            VmaAllocationCreateInfo alloc_info = {};
            alloc_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;
            alloc_info.requiredFlags = VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
            
            RenderTarget t;
            
            t.format = format;
            t.extent = size;
            VkImage local_image;
            vmaCreateImage(context.allocator, image_info, &alloc_info, &local_image, &t.allocation, nullptr);
            t.image = local_image;


            //Now create the image view
            vk::ImageViewCreateInfo image_view_info = {};
            image_view_info.image = t.image;
            image_view_info.format = t.format;
            image_view_info.viewType = vk::ImageViewType::e2D;
            //which of these should we expose?
            image_view_info.subresourceRange = {
                test,
                0,
                1,
                0,
                1
            };

            t.view = context.device.createImageView(image_view_info);
            return t;
        }
    
    public:
        RenderTargetManager(const Vulkan::Context& p_context): context(p_context),draw_color_target(RenderTarget()), draw_depth_target(RenderTarget()) {};
        ~RenderTargetManager()
        {
            if (draw_color_target.view != VK_NULL_HANDLE)
                context.device.destroyImageView(draw_color_target.view);
            if (draw_color_target.image != VK_NULL_HANDLE)
                vmaDestroyImage(context.allocator, draw_color_target.image, draw_color_target.allocation);
            if (draw_depth_target.view != VK_NULL_HANDLE)
                context.device.destroyImageView(draw_depth_target.view);
            if (draw_depth_target.image != VK_NULL_HANDLE)
                vmaDestroyImage(context.allocator, draw_depth_target.image, draw_depth_target.allocation);
        }
        RenderTargetManager() = delete;
        RenderTargetManager(const RenderTargetManager&) = delete;
        RenderTargetManager& operator=(const RenderTargetManager&) = delete;
        RenderTargetManager(RenderTargetManager&&) = delete;
        RenderTargetManager& operator=(RenderTargetManager&&) = delete;
        
        RenderTarget& createDrawColorTarget(const vk::Extent3D& size)
        {
            
            vk::ImageUsageFlags drawImageUsages{};
            drawImageUsages |= vk::ImageUsageFlags::BitsType::eTransferSrc;
            drawImageUsages |= vk::ImageUsageFlags::BitsType::eTransferDst;
            drawImageUsages |= vk::ImageUsageFlags::BitsType::eStorage;
            drawImageUsages |= vk::ImageUsageFlags::BitsType::eColorAttachment;
            

            draw_color_target = createRenderTarget(vk::ImageAspectFlags::BitsType::eColor,vk::Format::eR16G16B16A16Sfloat, size, drawImageUsages);

            return draw_color_target;
        }
    
        RenderTarget& createDepthTarget(const vk::Extent3D& size) 
        {
            vk::ImageUsageFlags depthImageUsages{};
            depthImageUsages |= vk::ImageUsageFlags::BitsType::eDepthStencilAttachment;
            
            draw_depth_target = createRenderTarget(vk::ImageAspectFlags::BitsType::eDepth,vk::Format::eD32Sfloat, size, depthImageUsages);

            return draw_depth_target;
        }

        RenderTarget& getDepthRenderTarget()
        {
            return draw_depth_target;
        }

        RenderTarget& getColorRenderTarget()
        {
            return draw_color_target;
        }
    };
}