module;

#include <vulkan/vulkan.hpp>
export module VulkanCommand;
import std;
import VulkanContext;
import ServiceLocator;

namespace Rendering::Vulkan
{
    //TODO: I think we want the ability to record across multiple threads maybe?
    export class CommandPoolManager : public ISystem
    {
    public:
        CommandPoolManager(const Vulkan::Context& context)
            :m_context(context)
        {	
        }
        ~CommandPoolManager()
        {
            for (uint32_t i = 0; i < FRAMES; i++)
            {
                m_context.device.destroyCommandPool(rendering_command.command_pools[i]);
            }
        }
        CommandPoolManager() = delete;
        CommandPoolManager(const CommandPoolManager&) = delete;
        CommandPoolManager& operator=(const CommandPoolManager&) = delete;
        CommandPoolManager(CommandPoolManager&&) = delete;
        CommandPoolManager& operator=(CommandPoolManager&&) = delete;
        

        vk::CommandBuffer& GetRenderingCommandBuffer(uint32_t frameIndex)
        {
            assert(frameIndex < FRAMES && "frameIndex is greater than the amount of frames possible.");
            return rendering_command.command_buffers[frameIndex];
        }
        
        vk::CommandBuffer& GetTransferCommandBuffer()
        {
            return transfer_command.command_buffer;
        }
        
        void BuildRenderCommandStructures()
        {
            vk::CommandPoolCreateInfo command_pool_info = {};
            command_pool_info.queueFamilyIndex = m_context.graphics_queue_index;
            
            for (uint32_t i = 0; i < FRAMES; ++i)
            {
                rendering_command.command_pools[i] = m_context.device.createCommandPool(command_pool_info, nullptr);
					
                vk::CommandBufferAllocateInfo command_buffer_info = {};
                command_buffer_info.commandPool = rendering_command.command_pools[i];
                command_buffer_info.commandBufferCount = 1;
                command_buffer_info.level = vk::CommandBufferLevel::ePrimary;

                //just grab the first for now
                rendering_command.command_buffers[i] = m_context.device.allocateCommandBuffers(command_buffer_info)[0];
            }
        }
        
        void BuildTransferCommandStructures()
        {
            vk::CommandPoolCreateInfo command_pool_info = {};
            command_pool_info.queueFamilyIndex = m_context.transfer_queue_index;
            
            transfer_command.command_pool = m_context.device.createCommandPool(command_pool_info, nullptr);
            
            vk::CommandBufferAllocateInfo command_buffer_info = {};
            command_buffer_info.commandPool = transfer_command.command_pool;
            command_buffer_info.commandBufferCount = 1;
            command_buffer_info.level = vk::CommandBufferLevel::ePrimary;
            
            transfer_command.command_buffer = m_context.device.allocateCommandBuffers(command_buffer_info)[0];
            
            
            
            vk::FenceCreateInfo fence_info = {};
            
            transfer_command.transfer_fence = m_context.device.createFence(fence_info);
        }
        
        void BeginRecordTransferCommands()
        {
            vk::CommandBufferBeginInfo begin_info = {};
            begin_info.flags = vk::CommandBufferUsageFlags::BitsType::eSimultaneousUse;
            transfer_command.command_buffer.begin(begin_info);
        }
        
        void EndRecordTransferCommands()
        {
            transfer_command.command_buffer.end();
        }
    private:
        const Vulkan::Context& m_context;
        //shared command pool allocation info
        
        struct RenderCommandStructures
        {
            vk::CommandPool command_pools[FRAMES];
            vk::CommandBuffer command_buffers[FRAMES];
        }rendering_command;

        
        struct TransferCommandStructures
        {
            vk::CommandBuffer command_buffer;
            vk::CommandPool command_pool;
            vk::Fence transfer_fence;
            std::vector<vk::Buffer> staging_buffers;
        }transfer_command;
    };

    
}

