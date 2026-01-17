module;
#include "vulkan/vulkan.hpp"
export module VulkanDescriptors;

import VulkanContext;
import ServiceLocator;
namespace Rendering::Vulkan
{
    // bindless design
    export class DescriptorManager: public ISystem
    {
    public:
        DescriptorManager() = delete;

        DescriptorManager(const Vulkan::Context& context)
            :m_context(context)
        {
            initialize();
        }

        vk::DescriptorPool& createDescriptorPool()
        {
            m_pool = m_context.device.createDescriptorPool(m_pool_create_info);
            m_allocate_info.descriptorPool = m_pool;
            return m_pool;
        }

        vk::DescriptorSetLayout& createDescriptorSetLayout()
        {
            m_layout = m_context.device.createDescriptorSetLayout(m_layout_create_info);
            return m_layout;
        }

        vk::DescriptorSet& createDescriptorSet()
        {
            if (m_context.device.allocateDescriptorSets(&m_allocate_info, &m_set) != vk::Result::eSuccess)
            {
                assert(true && "Descriptor set failed to allocate");
            }
            return m_set;
        }

        vk::DescriptorSet& getDescriptorSet()
        {
            return m_set;    
        }
        
        ~DescriptorManager()
        {
            
            m_context.device.destroyDescriptorSetLayout(m_layout);
            m_context.device.destroyDescriptorPool(m_pool);
            
        }
    private:
        //same here
        vk::DescriptorPool m_pool;
        vk::DescriptorSetLayoutCreateInfo m_layout_create_info;
        //we're going to need multiple
        vk::DescriptorSetLayout m_layout;
        vk::DescriptorSetAllocateInfo m_allocate_info;
        vk::DescriptorSet m_set;
        vk::DescriptorPoolCreateInfo m_pool_create_info;
        //TODO: update this because eUniformPool
        std::array<vk::DescriptorPoolSize, 2> m_pool_sizes;
        std::array<vk::DescriptorSetLayoutBinding, 2> m_bindings;
        std::array<vk::DescriptorBindingFlags, 2> m_binding_flags;
        vk::DescriptorSetLayoutBindingFlagsCreateInfo m_binding_flags_info;
        
        const Vulkan::Context& m_context;
        void initialize()
        {
            m_pool_create_info = vk::DescriptorPoolCreateInfo();
            m_pool_create_info.flags = vk::DescriptorPoolCreateFlagBits::eUpdateAfterBind;

            //TODO: we need to figure out what exact pool sizes we need
            m_pool_sizes[0].type = vk::DescriptorType::eCombinedImageSampler;
            m_pool_sizes[0].descriptorCount = 1000;

            m_pool_sizes[1].type = vk::DescriptorType::eStorageBuffer;
            m_pool_sizes[1].descriptorCount = 1000;
            //TODO: see below about eUniformBuffer
            /*
            m_pool_sizes[2].type = vk::DescriptorType::eUniformBuffer;
            m_pool_sizes[2].descriptorCount = 1000;
            */

            m_pool_create_info.maxSets = 1; // For bindless, typically one large set
            m_pool_create_info.poolSizeCount = static_cast<uint32_t>(m_pool_sizes.size());
            m_pool_create_info.pPoolSizes = m_pool_sizes.data();

            std::array<vk::DescriptorType, 2> types
            {
                vk::DescriptorType::eCombinedImageSampler,
                vk::DescriptorType::eStorageBuffer
                //TODO: this causes a validation layer error. Why?
                //vk::DescriptorType::eUniformBuffer
            };

            for (uint32_t i = 0; i < 2; ++i)
            {
                m_bindings[i].binding = i;
                m_bindings[i].descriptorType = types[i];
                m_bindings[i].descriptorCount = 1000;
                m_bindings[i].stageFlags = vk::ShaderStageFlagBits::eAll;
                m_binding_flags[i] = vk::DescriptorBindingFlagBits::eUpdateAfterBind;
            }

            m_binding_flags_info = vk::DescriptorSetLayoutBindingFlagsCreateInfo();
            m_binding_flags_info.bindingCount = m_binding_flags.size();
            m_binding_flags_info.pBindingFlags = m_binding_flags.data();

            m_layout_create_info = vk::DescriptorSetLayoutCreateInfo();;
            m_layout_create_info.bindingCount = m_bindings.size();
            m_layout_create_info.pBindings = m_bindings.data();
            m_layout_create_info.flags = vk::DescriptorSetLayoutCreateFlagBits::eUpdateAfterBindPool;
            m_layout_create_info.pNext = &m_binding_flags_info;

            // Allocate descriptor set
            m_allocate_info = vk::DescriptorSetAllocateInfo();
            m_allocate_info.descriptorSetCount = 1;
            m_allocate_info.pSetLayouts = &m_layout;
        }
    };
}
