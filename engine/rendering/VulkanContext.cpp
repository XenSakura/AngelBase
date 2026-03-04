module;
#pragma once
#include <vulkan/vulkan.hpp>
#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"
#include "GLFW/glfw3.h"
#include "glm/glm.hpp"
export module VulkanContext;

export namespace Rendering::Vulkan
{
    
    constexpr uint32_t FRAMES = 2;
    
    struct Context
    {
        
        vk::Instance instance;
        vk::DebugUtilsMessengerEXT debug_callback;
        vk::Device device;
        vk::PhysicalDevice physical_device;
        vk::Queue graphics_queue;
        vk::SurfaceKHR surface;
        VmaAllocator vram_allocator;
        int32_t graphics_queue_index = -1;
        int32_t transfer_queue_index = -1;
        std::vector<vk::Semaphore> rendering_semaphores;
    };

    struct Image
    {
        vk::Image image;
        vk::ImageView view;
        VmaAllocation allocation;
        vk::Extent3D extent;
        vk::Format format;
    };

    
}
