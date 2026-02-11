module;
#include <vulkan/vulkan.hpp>
export module VulkanBuffer;

namespace Rendering::Vulkan
{
    export class BufferManager
    {
    private:
        std::vector<vk::Buffer> buffers;
    public:

        void createBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage)
        {
            vk::BufferCreateInfo bufferCreateInfo;
            bufferCreateInfo.size = size;
            bufferCreateInfo.usage = usage;
        }
    };
}