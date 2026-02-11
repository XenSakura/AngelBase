module;
#include "vulkan/vulkan.hpp"
export module VulkanDescriptors;

import VulkanContext;
import ServiceLocator;
namespace Rendering::Vulkan
{
    // bindless design
    //TODO: Sakura, we're only using descriptors for the textures, otherwise buffer device address
    //TODO: Descriptors are only used for ImageSamplers/textures so we need to refactor
    export class DescriptorManager: public ISystem
    {
    public:
        DescriptorManager() = delete;

        DescriptorManager(const Vulkan::Context& context)
            :m_context(context)
        {
            initialize();
        }
        
        ~DescriptorManager()
        {
            
            m_context.device.destroyDescriptorSetLayout(m_layout);
            m_context.device.destroyDescriptorPool(m_pool);
            
        }
    private:
        vk::DescriptorSetLayoutCreateInfo m_layout_create_info;
        //we're going to need 1
        vk::DescriptorSetLayout m_layout;
       
        vk::DescriptorBindingFlags m_binding_flags;
        vk::DescriptorSetLayoutBindingFlagsCreateInfo m_desc_binding_flags_create_info;
        vk::DescriptorSetLayoutBinding m_layout_binding;
        
        vk::DescriptorPool m_pool;
        
        const Vulkan::Context& m_context;
        void initialize()
        {
            //only ever need one descriptor pool/set since the GPU will use it all
            
            // creating the descriptor set layouts just for the textures
            m_binding_flags = vk::DescriptorBindingFlagBits::eVariableDescriptorCount;

            m_desc_binding_flags_create_info.bindingCount = 1;
            m_desc_binding_flags_create_info.pBindingFlags = &m_binding_flags;
            
            m_layout_binding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
            //TODO: size of total textures
            m_layout_binding.descriptorCount = 1;
            m_layout_binding.stageFlags = vk::ShaderStageFlagBits::eFragment;

            m_layout_create_info = vk::DescriptorSetLayoutCreateInfo();
            m_layout_create_info.bindingCount = 1;
            m_layout_create_info.pBindings = &m_layout_binding;
            m_layout_create_info.pNext = &m_desc_binding_flags_create_info;

            if (m_context.device.createDescriptorSetLayout(&m_layout_create_info, nullptr, &m_layout) != vk::Result::eSuccess)
            {
                assert(false && "failed to create descriptor set layout");
            }

            std::vector<vk::DescriptorPoolSize> poolSizes = {
                // only allocate descriptors for images/ textures
                vk::DescriptorPoolSize(vk::DescriptorType::eCombinedImageSampler, 1)
            };
            

            vk::DescriptorPoolCreateInfo poolCreateInfo = {};
            poolCreateInfo.poolSizeCount = 1;
            poolCreateInfo.pPoolSizes = poolSizes.data();
            //TODO: not sure if this is correct
            poolCreateInfo.maxSets = FRAMES;

            if (m_context.device.createDescriptorPool(&poolCreateInfo, nullptr, &m_pool) != vk::Result::eSuccess)
            {
                assert(false && "failed to create descriptor pool");
            }

            //TODO: as above, texture size
            uint32_t descriptor_count = 1;
            vk::DescriptorSetVariableDescriptorCountAllocateInfo variable_desc_count_info = {};
            variable_desc_count_info.descriptorSetCount = 1;
            variable_desc_count_info.pDescriptorCounts = &descriptor_count;

            vk::DescriptorSetAllocateInfo allocateInfo = {};
            allocateInfo.descriptorPool = m_pool;
            allocateInfo.pSetLayouts = &m_layout;
            allocateInfo.descriptorSetCount = 1;
            allocateInfo.pNext = &variable_desc_count_info;


            vk::DescriptorSet set;
            // Allocate descriptor set-- we are only allocating 1 for images
            set = m_context.device.allocateDescriptorSets(allocateInfo)[0];

            std::vector<vk::WriteDescriptorSet> write_descriptor_sets = {
                //vk::WriteDescriptorSet(set, vk::DescriptorType::eCombinedImageSampler, 0, &Texture.sampler)
            };

            m_context.device.updateDescriptorSets(static_cast<uint32_t>(write_descriptor_sets.size()), write_descriptor_sets.data(), 0, nullptr);
            
        }
    };
}
