module;
#include <vulkan/vulkan.hpp>

export module VulkanPipeline;
import ServiceLocator;
import VulkanContext;
import RenderTargetManager;

import std;

namespace Rendering::Vulkan
{
    export class PipelineManager : public ISystem
    {
    public:
        PipelineManager() = delete;
        PipelineManager(const Context& context) : m_context(context) {}

        ~PipelineManager()
        {
            cleanup();
        }

        // Nested PipelineBuilder class
        class PipelineBuilder
        {
        public:
            enum class PipelineType {
                Graphics,
                Compute,
                RayTracing
            };

        private:
            PipelineType m_type;
            const Context& m_context;
            
            // Shader stages (common to all pipeline types)
            std::vector<vk::PipelineShaderStageCreateInfo> m_shaderStages;
            
            // Graphics-specific state
            vk::PipelineVertexInputStateCreateInfo m_vertexInput;
            std::vector<vk::VertexInputBindingDescription> m_vertexBindings;
            std::vector<vk::VertexInputAttributeDescription> m_vertexAttributes;
            vk::PipelineInputAssemblyStateCreateInfo m_inputAssembly;
            vk::PipelineViewportStateCreateInfo m_viewportState;
            vk::PipelineRasterizationStateCreateInfo m_rasterization;
            vk::PipelineDepthStencilStateCreateInfo m_depthStencil;
            vk::PipelineColorBlendStateCreateInfo m_colorBlend;
            vk::PipelineColorBlendAttachmentState m_colorBlendAttachment;
            vk::PipelineMultisampleStateCreateInfo m_multisample;
            vk::PipelineDynamicStateCreateInfo m_dynamicState;
            vk::PipelineRenderingCreateInfo m_renderingInfo;
            
            // Ray tracing specific
            std::vector<vk::RayTracingShaderGroupCreateInfoKHR> m_rtShaderGroups;
            uint32_t m_maxRecursionDepth = 1;
            
            // Common properties
            vk::PipelineLayout m_layout;
            std::vector<vk::DynamicState> m_dynamicStates;
            std::vector<vk::Format> m_colorFormats;
            vk::Format m_depthFormat = vk::Format::eUndefined;
            
        public:
            PipelineBuilder(const Context& context) 
                : m_type(PipelineType::Graphics), m_context(context) 
            {
                setDefaults();
            }
            
            // Type selection
            PipelineBuilder& setPipelineType(PipelineType type) {
                m_type = type;
                return *this;
            }
            
            // Shader stage management
            PipelineBuilder& addShaderStage(vk::ShaderStageFlagBits stage, 
                                           vk::ShaderModule module, 
                                           const char* entryPoint = "main") {
                vk::PipelineShaderStageCreateInfo stageInfo{};
                stageInfo.stage = stage;
                stageInfo.module = module;
                stageInfo.pName = entryPoint;
                m_shaderStages.push_back(stageInfo);
                return *this;
            }
            
            PipelineBuilder& clearShaderStages() {
                m_shaderStages.clear();
                return *this;
            }
            
            // Vertex input configuration
            PipelineBuilder& addVertexBinding(uint32_t binding, uint32_t stride, 
                                             vk::VertexInputRate inputRate = vk::VertexInputRate::eVertex) {
                vk::VertexInputBindingDescription bindingDesc{};
                bindingDesc.binding = binding;
                bindingDesc.stride = stride;
                bindingDesc.inputRate = inputRate;
                m_vertexBindings.push_back(bindingDesc);
                return *this;
            }
            
            PipelineBuilder& addVertexAttribute(uint32_t location, uint32_t binding, 
                                               vk::Format format, uint32_t offset) {
                vk::VertexInputAttributeDescription attrDesc{};
                attrDesc.location = location;
                attrDesc.binding = binding;
                attrDesc.format = format;
                attrDesc.offset = offset;
                m_vertexAttributes.push_back(attrDesc);
                return *this;
            }
            
            // Graphics-specific methods
            PipelineBuilder& setInputTopology(vk::PrimitiveTopology topology) {
                m_inputAssembly.topology = topology;
                m_inputAssembly.primitiveRestartEnable = VK_FALSE;
                return *this;
            }
            
            PipelineBuilder& setPolygonMode(vk::PolygonMode mode) {
                m_rasterization.polygonMode = mode;
                return *this;
            }
            
            PipelineBuilder& setCullMode(vk::CullModeFlags cullMode, vk::FrontFace frontFace) {
                m_rasterization.cullMode = cullMode;
                m_rasterization.frontFace = frontFace;
                return *this;
            }
            
            PipelineBuilder& setDepthTest(bool enable, vk::CompareOp compareOp = vk::CompareOp::eLess) {
                m_depthStencil.depthTestEnable = enable;
                m_depthStencil.depthWriteEnable = enable;
                m_depthStencil.depthCompareOp = compareOp;
                return *this;
            }
            
            PipelineBuilder& setColorFormats(const std::vector<vk::Format>& formats) {
                m_colorFormats = formats;
                m_renderingInfo.colorAttachmentCount = static_cast<uint32_t>(formats.size());
                m_renderingInfo.pColorAttachmentFormats = m_colorFormats.data();
                return *this;
            }
            
            PipelineBuilder& setDepthFormat(vk::Format format) {
                m_depthFormat = format;
                m_renderingInfo.depthAttachmentFormat = format;
                return *this;
            }
            
            PipelineBuilder& addDynamicState(vk::DynamicState state) {
                m_dynamicStates.push_back(state);
                return *this;
            }
            
            PipelineBuilder& setMultisampling(vk::SampleCountFlagBits samples) {
                m_multisample.rasterizationSamples = samples;
                return *this;
            }
            
            PipelineBuilder& setBlending(bool enable, vk::BlendFactor srcColor = vk::BlendFactor::eSrcAlpha,
                                        vk::BlendFactor dstColor = vk::BlendFactor::eOneMinusSrcAlpha,
                                        vk::BlendOp colorOp = vk::BlendOp::eAdd) {
                m_colorBlendAttachment.blendEnable = enable;
                if (enable) {
                    m_colorBlendAttachment.srcColorBlendFactor = srcColor;
                    m_colorBlendAttachment.dstColorBlendFactor = dstColor;
                    m_colorBlendAttachment.colorBlendOp = colorOp;
                    m_colorBlendAttachment.srcAlphaBlendFactor = vk::BlendFactor::eOne;
                    m_colorBlendAttachment.dstAlphaBlendFactor = vk::BlendFactor::eZero;
                    m_colorBlendAttachment.alphaBlendOp = vk::BlendOp::eAdd;
                }
                return *this;
            }
            
            // Ray tracing specific methods
            PipelineBuilder& addRayTracingShaderGroup(const vk::RayTracingShaderGroupCreateInfoKHR& group) {
                m_rtShaderGroups.push_back(group);
                return *this;
            }
            
            PipelineBuilder& setMaxRecursionDepth(uint32_t depth) {
                m_maxRecursionDepth = depth;
                return *this;
            }
            
            // Common layout
            PipelineBuilder& setLayout(vk::PipelineLayout layout) {
                m_layout = layout;
                return *this;
            }
            
            // Build method - returns pipeline for manager to cache
            vk::Pipeline build() {
                switch (m_type) {
                    case PipelineType::Graphics:
                        return buildGraphics();
                    case PipelineType::Compute:
                        return buildCompute();
                    //TODO:
                    case PipelineType::RayTracing:
                        return buildGraphics();
                }
                return nullptr;
            }
            
        private:
            void setDefaults() {
                // Input assembly defaults
                m_inputAssembly.topology = vk::PrimitiveTopology::eTriangleList;
                m_inputAssembly.primitiveRestartEnable = VK_FALSE;
                
                // Rasterization defaults
                m_rasterization.polygonMode = vk::PolygonMode::eFill;
                m_rasterization.cullMode = vk::CullModeFlagBits::eBack;
                m_rasterization.frontFace = vk::FrontFace::eClockwise;
                m_rasterization.lineWidth = 1.0f;
                
                // Depth stencil defaults
                m_depthStencil.depthTestEnable = VK_TRUE;
                m_depthStencil.depthWriteEnable = VK_TRUE;
                m_depthStencil.depthCompareOp = vk::CompareOp::eLess;
                
                // Color blend defaults
                m_colorBlendAttachment.blendEnable = VK_FALSE;
                m_colorBlendAttachment.colorWriteMask = 
                    vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
                    vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
                
                // Multisampling defaults
                m_multisample.rasterizationSamples = vk::SampleCountFlagBits::e1;
                m_multisample.minSampleShading = 1.0f;
            }
            
            vk::Pipeline buildGraphics() {
                // Setup vertex input state
                m_vertexInput.vertexBindingDescriptionCount = static_cast<uint32_t>(m_vertexBindings.size());
                m_vertexInput.pVertexBindingDescriptions = m_vertexBindings.data();
                m_vertexInput.vertexAttributeDescriptionCount = static_cast<uint32_t>(m_vertexAttributes.size());
                m_vertexInput.pVertexAttributeDescriptions = m_vertexAttributes.data();
                
                // Setup dynamic state
                if (!m_dynamicStates.empty()) {
                    m_dynamicState.dynamicStateCount = static_cast<uint32_t>(m_dynamicStates.size());
                    m_dynamicState.pDynamicStates = m_dynamicStates.data();
                }
                
                // Setup color blend
                m_colorBlend.attachmentCount = 1;
                m_colorBlend.pAttachments = &m_colorBlendAttachment;
                
                // Setup viewport state (typically dynamic)
                m_viewportState.viewportCount = 1;
                m_viewportState.scissorCount = 1;
                
                vk::GraphicsPipelineCreateInfo pipelineInfo{};
                pipelineInfo.pNext = &m_renderingInfo;
                pipelineInfo.stageCount = static_cast<uint32_t>(m_shaderStages.size());
                pipelineInfo.pStages = m_shaderStages.data();
                pipelineInfo.pVertexInputState = &m_vertexInput;
                pipelineInfo.pInputAssemblyState = &m_inputAssembly;
                pipelineInfo.pViewportState = &m_viewportState;
                pipelineInfo.pRasterizationState = &m_rasterization;
                pipelineInfo.pMultisampleState = &m_multisample;
                pipelineInfo.pDepthStencilState = &m_depthStencil;
                pipelineInfo.pColorBlendState = &m_colorBlend;
                pipelineInfo.pDynamicState = m_dynamicStates.empty() ? nullptr : &m_dynamicState;
                pipelineInfo.layout = m_layout;
                
                auto result = m_context.device.createGraphicsPipeline(nullptr, pipelineInfo);
                if (result.result != vk::Result::eSuccess) {
                    throw std::runtime_error("Failed to create graphics pipeline");
                }
                return result.value;
            }
            
            vk::Pipeline buildCompute() {
                if (m_shaderStages.empty() || m_shaderStages[0].stage != vk::ShaderStageFlagBits::eCompute) {
                    throw std::runtime_error("Compute pipeline requires a compute shader stage");
                }
                
                vk::ComputePipelineCreateInfo pipelineInfo{};
                pipelineInfo.stage = m_shaderStages[0];
                pipelineInfo.layout = m_layout;
                
                auto result = m_context.device.createComputePipeline(nullptr, pipelineInfo);
                if (result.result != vk::Result::eSuccess) {
                    throw std::runtime_error("Failed to create compute pipeline");
                }
                return result.value;
            }
            /*
            vk::Pipeline buildRayTracing() {
                vk::RayTracingPipelineCreateInfoKHR pipelineInfo{};
                pipelineInfo.stageCount = static_cast<uint32_t>(m_shaderStages.size());
                pipelineInfo.pStages = m_shaderStages.data();
                pipelineInfo.groupCount = static_cast<uint32_t>(m_rtShaderGroups.size());
                pipelineInfo.pGroups = m_rtShaderGroups.data();
                pipelineInfo.maxPipelineRayRecursionDepth = m_maxRecursionDepth;
                pipelineInfo.layout = m_layout;
                
                auto result = m_context.device.createRayTracingPipelineKHR(nullptr, nullptr, pipelineInfo);
                if (result.result != vk::Result::eSuccess) {
                    throw std::runtime_error("Failed to create ray tracing pipeline");
                }
                return result.value;
            }*/
        };

        // Get a builder instance
        PipelineBuilder getBuilder() {
            return PipelineBuilder(m_context);
        }

        // Cache and manage pipeline lifetime
        vk::Pipeline cachePipeline(const std::string& name, vk::Pipeline pipeline, vk::PipelineLayout layout) {
            m_pipelines[name] = pipeline;
            m_pipelineLayouts[name] = layout;
            return pipeline;
        }

        // Retrieve cached pipeline by name
        vk::Pipeline getPipeline(const std::string& name) const {
            auto it = m_pipelines.find(name);
            if (it != m_pipelines.end()) {
                return it->second;
            }
            return nullptr;
        }

        // Retrieve cached pipeline layout by name
        vk::PipelineLayout getPipelineLayout(const std::string& name) const {
            auto it = m_pipelineLayouts.find(name);
            if (it != m_pipelineLayouts.end()) {
                return it->second;
            }
            return nullptr;
        }

        // Create and cache pipeline layout
        vk::PipelineLayout createPipelineLayout(const std::string& name,
                                               const std::vector<vk::DescriptorSetLayout>& setLayouts,
                                               const std::vector<vk::PushConstantRange>& pushConstants = {}) {
            vk::PipelineLayoutCreateInfo layoutInfo{};
            layoutInfo.setLayoutCount = static_cast<uint32_t>(setLayouts.size());
            layoutInfo.pSetLayouts = setLayouts.data();
            layoutInfo.pushConstantRangeCount = static_cast<uint32_t>(pushConstants.size());
            layoutInfo.pPushConstantRanges = pushConstants.data();

            vk::PipelineLayout layout = m_context.device.createPipelineLayout(layoutInfo);
            m_pipelineLayouts[name] = layout;
            return layout;
        }

        // Check if pipeline exists
        bool hasPipeline(const std::string& name) const {
            return m_pipelines.find(name) != m_pipelines.end();
        }

        // Remove specific pipeline
        void removePipeline(const std::string& name) {
            auto pipelineIt = m_pipelines.find(name);
            if (pipelineIt != m_pipelines.end()) {
                m_context.device.destroyPipeline(pipelineIt->second);
                m_pipelines.erase(pipelineIt);
            }

            auto layoutIt = m_pipelineLayouts.find(name);
            if (layoutIt != m_pipelineLayouts.end()) {
                m_context.device.destroyPipelineLayout(layoutIt->second);
                m_pipelineLayouts.erase(layoutIt);
            }
        }

        // Cleanup all pipelines
        void cleanup() {
            for (auto& [name, pipeline] : m_pipelines) {
                m_context.device.destroyPipeline(pipeline);
            }
            m_pipelines.clear();

            for (auto& [name, layout] : m_pipelineLayouts) {
                m_context.device.destroyPipelineLayout(layout);
            }
            m_pipelineLayouts.clear();
        }

    private:
        const Context& m_context;
        std::unordered_map<std::string, vk::Pipeline> m_pipelines;
        std::unordered_map<std::string, vk::PipelineLayout> m_pipelineLayouts;
    };
}