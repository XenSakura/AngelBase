module;

#include <vulkan/vulkan.h>
export module vk_command;
import std;

struct CommandStructures
{
    VkCommandPool _commandPool;
    VkCommandBuffer _commandBuffer;
};


//TODO: I think we want the ability to record across multiple threads maybe?
export class CommandPoolManager
{
    
};

//handles building command buffers to record various commands to
export class CommandBufferBuilder
{
    
};