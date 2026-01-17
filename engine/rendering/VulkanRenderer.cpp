module;

#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include <vulkan/vulkan.hpp>
#include "vk_mem_alloc.h"

#define GLFW_INCLUDE_NONE
#include "assimp/Vertex.h"
#include "GLFW/glfw3.h"


export module VulkanRenderer;

import std;
import VulkanContext;
import VulkanExtensions;
import WindowManager;
import VulkanSwapchain;
import RenderTargetManager;
import VulkanCommand;
import VulkanDescriptors;
import ServiceLocator;
import ShaderManager;
import VulkanPipeline;
VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE



namespace Rendering::Vulkan
{
		// LIFO deletion stack for async GPU cleanup
	struct DeleteStack
	{
		std::stack<std::function<void()>> delete_functions;

		void push(std::function<void()>&& func)
		{
			delete_functions.push(std::move(func));
		}

		void clear()
		{
			while (!delete_functions.empty())
			{
				delete_functions.top()();
				delete_functions.pop();
			}
		}
	};

	
	export class VulkanRenderer
	{
	private:
		Context m_context = Context();
		DeleteStack m_destructor;
		
		// can hold multiple
		std::shared_ptr<SwapchainManager> m_swapchain_manager;
		// can hold multiple
		std::shared_ptr<RenderTargetManager> m_render_target_manager;
		//can hold multiple
		std::shared_ptr<WindowManager> m_window_manager;
		// a la the same
		std::shared_ptr<CommandPoolManager> m_command_pool_manager;

		std::shared_ptr<DescriptorManager> m_descriptor_manager;

		std::shared_ptr<PipelineManager> m_pipeline_manager;

		uint32_t current_frame = 0;

		struct FrameResources
		{
			vk::Semaphore semaphore;
			vk::Fence fence;
			DeleteStack delete_stack;
		} frame_resources[FRAMES];

	public:
		bool initialize(uint32_t width, uint32_t height)
		{
			m_window_manager = std::make_shared<WindowManager>();
			ServiceLocator::Instance()->RegisterSystem<WindowManager>(m_window_manager);
			m_window_manager->ResizeWindow(2560, 1440);
			//1. Init instance
			{
				// Setup instance extensions
				std::vector<const char*> instance_extensions{VK_KHR_SURFACE_EXTENSION_NAME};
				VulkanExtensions::addInstanceExtensions(instance_extensions);

				// Initialize GLFW and add its required extensions
		
		
				uint32_t glfw_extension_count = 0;
				const char** glfw_extensions = glfwGetRequiredInstanceExtensions(&glfw_extension_count);
				instance_extensions.insert(instance_extensions.end(), 
										   glfw_extensions, 
										   glfw_extensions + glfw_extension_count);

				// Setup validation layers
				std::vector<const char*> instance_layers;
				VulkanExtensions::addInstanceLayers(instance_layers);
		
				// Create application info
				vk::ApplicationInfo app_info;
				app_info.setPApplicationName("AngelBase");
				app_info.setApplicationVersion(vk::makeVersion(1, 0, 0));
				app_info.setPEngineName("AngelBase");
				app_info.setEngineVersion(vk::makeVersion(1, 0, 0));
				app_info.setApiVersion(vk::ApiVersion14);

				// Create instance
				vk::InstanceCreateInfo instance_info;
				instance_info.setPApplicationInfo(&app_info);
				instance_info.setPEnabledLayerNames(instance_layers);
				instance_info.setPEnabledExtensionNames(instance_extensions);

				// Create dynamic dispatcher for extension functions
				vk::detail::DynamicLoader dl;
				PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr = 
				dl.getProcAddress<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr");
				VULKAN_HPP_DEFAULT_DISPATCHER.init(vkGetInstanceProcAddr);
		
				// Add debug messenger to instance creation
				auto debug_info = VulkanExtensions::createDebugMessengerInfo();
				if (debug_info)
					instance_info.setPNext(&(*debug_info));

				m_context.instance = vk::createInstance(instance_info);

				VULKAN_HPP_DEFAULT_DISPATCHER.init(m_context.instance);
		
				// Create persistent debug messenger
				if (debug_info)
					m_context.debug_callback = m_context.instance.createDebugUtilsMessengerEXT(*debug_info);
			}
			
			//2. Init surface
			{
				if (m_context.surface)
					m_context.instance.destroySurfaceKHR(m_context.surface);
				m_context.surface = m_window_manager.get()->VulkanCreateWindowSurface(m_context.instance);
			}
			
			
			//3. Initialize devices
			{
				const auto physical_devices = m_context.instance.enumeratePhysicalDevices();

				for (const auto& device : physical_devices)
				{
					const auto properties = device.getProperties();
			
					// Check API version
					if (properties.apiVersion < vk::ApiVersion14)
					{
						std::cerr << std::format("Physical Device '{}' does not support Vulkan 1.4\n",
												properties.deviceName.data());
						continue;
					}

					// Find suitable queue family
					const auto queue_families = device.getQueueFamilyProperties();
			
					for (size_t i = 0; i < queue_families.size(); ++i)
					{
						const auto& family = queue_families[i];
						const bool has_graphics = static_cast<bool>(family.queueFlags & vk::QueueFlagBits::eGraphics);
						const bool has_present = device.getSurfaceSupportKHR(static_cast<uint32_t>(i), m_context.surface);

						if (has_graphics && has_present)
						{
							m_context.graphics_queue_index = static_cast<int32_t>(i);
							m_context.physical_device = device;
							break;
						}
					}
				}

				if (m_context.physical_device == VK_NULL_HANDLE)
					throw std::runtime_error("Failed to find suitable GPU with Vulkan 1.4 support");

				// Validate device extensions
				const auto device_extensions = m_context.physical_device.enumerateDeviceExtensionProperties();
				const std::vector<const char*> required_extensions
				{
					VK_KHR_SWAPCHAIN_EXTENSION_NAME,
					// ray tracing 
					VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
					VK_KHR_RAY_QUERY_EXTENSION_NAME,
					VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
					// deferred multithreading things
					VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
					// query available VRAM in real time
					VK_EXT_MEMORY_BUDGET_EXTENSION_NAME,
					// to reduce shader compilation time
					VK_KHR_PIPELINE_LIBRARY_EXTENSION_NAME,
					VK_EXT_GRAPHICS_PIPELINE_LIBRARY_EXTENSION_NAME,
					// allows paging of GPU VRAM to better use GPU memory
					VK_EXT_MEMORY_PRIORITY_EXTENSION_NAME,
					VK_EXT_PAGEABLE_DEVICE_LOCAL_MEMORY_EXTENSION_NAME,
					//render at diff resolutions
					VK_KHR_FRAGMENT_SHADING_RATE_EXTENSION_NAME,
					//mesh shaders, replaces vertex and geometry shaders
					VK_EXT_MESH_SHADER_EXTENSION_NAME
				};

				if (!VulkanExtensions::validateExtensions(required_extensions, device_extensions))
					throw std::runtime_error("Required device extensions are missing");

				// Check required features
				const auto available_features = m_context.physical_device.getFeatures2<
					vk::PhysicalDeviceFeatures2,
					vk::PhysicalDeviceVulkan14Features,
					vk::PhysicalDeviceVulkan13Features,
					vk::PhysicalDeviceVulkan12Features,
					vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT,
					vk::PhysicalDevicePageableDeviceLocalMemoryFeaturesEXT,
					vk::PhysicalDeviceRayTracingPipelineFeaturesKHR,
					vk::PhysicalDeviceAccelerationStructureFeaturesKHR,
					vk::PhysicalDeviceMeshShaderFeaturesEXT,
					vk::PhysicalDeviceFragmentShadingRateFeaturesKHR>();

				const auto& vulkan14 = available_features.get<vk::PhysicalDeviceVulkan14Features>();
			    const auto& vulkan13 = available_features.get<vk::PhysicalDeviceVulkan13Features>();
			    const auto& vulkan12 = available_features.get<vk::PhysicalDeviceVulkan12Features>();
			    const auto& extended = available_features.get<vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>();
			    const auto& pageable = available_features.get<vk::PhysicalDevicePageableDeviceLocalMemoryFeaturesEXT>();
			    const auto& ray_tracing = available_features.get<vk::PhysicalDeviceRayTracingPipelineFeaturesKHR>();
			    const auto& accel_struct = available_features.get<vk::PhysicalDeviceAccelerationStructureFeaturesKHR>();
			    const auto& mesh_shader = available_features.get<vk::PhysicalDeviceMeshShaderFeaturesEXT>();
			    const auto& vrs = available_features.get<vk::PhysicalDeviceFragmentShadingRateFeaturesKHR>();

			    // Core features
			    if (!vulkan13.dynamicRendering)
			        throw std::runtime_error("Dynamic rendering feature is missing");
			    if (!vulkan13.synchronization2)
			        throw std::runtime_error("Synchronization2 feature is missing");
			    if (!vulkan12.bufferDeviceAddress)
			        throw std::runtime_error("Buffer device address feature is missing");
			    if (!vulkan12.descriptorIndexing)
			        throw std::runtime_error("Descriptor indexing feature is missing");
			    if (!vulkan12.timelineSemaphore)
			        throw std::runtime_error("Timeline semaphore feature is missing");
			    if (!vulkan14.pushDescriptor)
			        throw std::runtime_error("Push descriptor feature is missing");
			    if (!vulkan14.hostImageCopy)
			        throw std::runtime_error("Host image copy feature is missing");
			    if (!extended.extendedDynamicState)
			        throw std::runtime_error("Extended dynamic state feature is missing");

			    // Extension features
			    if (!pageable.pageableDeviceLocalMemory)
			        throw std::runtime_error("Pageable device local memory feature is missing");
			    if (!ray_tracing.rayTracingPipeline)
			        throw std::runtime_error("Ray tracing pipeline feature is missing");
			    if (!accel_struct.accelerationStructure)
			        throw std::runtime_error("Acceleration structure feature is missing");
			    if (!mesh_shader.meshShader || !mesh_shader.taskShader)
			        throw std::runtime_error("Mesh shader features are missing");
			    if (!vrs.pipelineFragmentShadingRate)
			        throw std::runtime_error("Fragment shading rate feature is missing");

			    // Enable required features
			    vk::StructureChain<
			        vk::PhysicalDeviceFeatures2,
			        vk::PhysicalDeviceVulkan14Features,
			        vk::PhysicalDeviceVulkan13Features,
			        vk::PhysicalDeviceVulkan12Features,
			        vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT,
			        vk::PhysicalDevicePageableDeviceLocalMemoryFeaturesEXT,
			        vk::PhysicalDeviceRayTracingPipelineFeaturesKHR,
			        vk::PhysicalDeviceAccelerationStructureFeaturesKHR,
			        vk::PhysicalDeviceMeshShaderFeaturesEXT,
			        vk::PhysicalDeviceFragmentShadingRateFeaturesKHR> enabled_features;

			    auto& enabled_vulkan14 = enabled_features.get<vk::PhysicalDeviceVulkan14Features>();
			    enabled_vulkan14.setPushDescriptor(VK_TRUE);
			    enabled_vulkan14.setHostImageCopy(VK_TRUE);
			    
			    auto& enabled_vulkan13 = enabled_features.get<vk::PhysicalDeviceVulkan13Features>();
			    enabled_vulkan13.setSynchronization2(VK_TRUE);
			    enabled_vulkan13.setDynamicRendering(VK_TRUE);
			    
			    auto& enabled_vulkan12 = enabled_features.get<vk::PhysicalDeviceVulkan12Features>();
			    enabled_vulkan12.setBufferDeviceAddress(VK_TRUE);
			    enabled_vulkan12.setDescriptorIndexing(VK_TRUE);
			    enabled_vulkan12.setTimelineSemaphore(VK_TRUE);

				// allows descriptor arrays where we don't fill all slots
				enabled_vulkan12.setDescriptorBindingPartiallyBound(VK_TRUE);
				//allows unbounded arrays in shaders
				enabled_vulkan12.setRuntimeDescriptorArray(VK_TRUE);
				// Allows allocating fewer descriptors than the max
				enabled_vulkan12.setDescriptorBindingVariableDescriptorCount(VK_TRUE);
				// allows dynamic indexing with the non uniform indices
				enabled_vulkan12.setShaderSampledImageArrayNonUniformIndexing(VK_TRUE);
				// allows updating descriptors when they're in use
				enabled_vulkan12.setDescriptorBindingSampledImageUpdateAfterBind(VK_TRUE);

				enabled_vulkan12.setShaderStorageBufferArrayNonUniformIndexing(VK_TRUE);
				enabled_vulkan12.setDescriptorBindingStorageBufferUpdateAfterBind(VK_TRUE);

			    auto& enabled_extended = enabled_features.get<vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>();
			    enabled_extended.setExtendedDynamicState(VK_TRUE);

			    auto& enabled_pageable = enabled_features.get<vk::PhysicalDevicePageableDeviceLocalMemoryFeaturesEXT>();
			    enabled_pageable.setPageableDeviceLocalMemory(VK_TRUE);

			    auto& enabled_ray_tracing = enabled_features.get<vk::PhysicalDeviceRayTracingPipelineFeaturesKHR>();
			    enabled_ray_tracing.setRayTracingPipeline(VK_TRUE);

			    auto& enabled_accel = enabled_features.get<vk::PhysicalDeviceAccelerationStructureFeaturesKHR>();
			    enabled_accel.setAccelerationStructure(VK_TRUE);

			    auto& enabled_mesh = enabled_features.get<vk::PhysicalDeviceMeshShaderFeaturesEXT>();
			    enabled_mesh.setMeshShader(VK_TRUE);
			    enabled_mesh.setTaskShader(VK_TRUE);

			    auto& enabled_vrs = enabled_features.get<vk::PhysicalDeviceFragmentShadingRateFeaturesKHR>();
			    enabled_vrs.setPipelineFragmentShadingRate(VK_TRUE);

				// Create queue
				constexpr float queue_priority = 0.5f;
				vk::DeviceQueueCreateInfo queue_info;
				queue_info.setQueueFamilyIndex(static_cast<uint32_t>(m_context.graphics_queue_index));
				queue_info.setQueueCount(1);
				queue_info.setPQueuePriorities(&queue_priority);

				// Create logical device
				vk::DeviceCreateInfo device_info;
				device_info.setPNext(&enabled_features.get<vk::PhysicalDeviceFeatures2>());
				device_info.setQueueCreateInfos(queue_info);
				device_info.setPEnabledExtensionNames(required_extensions);

				m_context.device = m_context.physical_device.createDevice(device_info);
				m_context.graphics_queue = m_context.device.getQueue(m_context.graphics_queue_index, 0);
			}

			//4. Initialize Allocator
			{
				VmaAllocatorCreateInfo allocator_info = {};
				allocator_info.physicalDevice = m_context.physical_device;
				allocator_info.instance = m_context.instance;
				allocator_info.device = m_context.device;
				allocator_info.flags= VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
				vmaCreateAllocator(&allocator_info, &m_context.allocator);

				m_destructor.push([&]()
				{
					vmaDestroyAllocator(m_context.allocator);
				});
			}

			//5. initialize and create swapchain
			{
				m_swapchain_manager = std::make_shared<SwapchainManager>(m_context);
				ServiceLocator::Instance()->RegisterSystem(m_swapchain_manager);
				m_swapchain_manager.get()->createSwapchain(width, height);
			}

			//6. Create rendering semaphores
			{
				Swapchain m_swapchain = m_swapchain_manager.get()->getSwapchain();
				
				m_context.rendering_semaphores.resize(m_swapchain.images.size());

				vk::SemaphoreCreateInfo semaphore_info = {};

				for (uint32_t i = 0; i < m_swapchain.images.size(); ++i)
				{
					m_context.rendering_semaphores[i] = m_context.device.createSemaphore(semaphore_info, nullptr);
				}

				m_destructor.push([&]()
				{
					for (vk::Semaphore& a : m_context.rendering_semaphores)
					{
						m_context.device.destroySemaphore(a);
					}
					m_context.rendering_semaphores.clear();
				});
			}

			//7. Create draw and depth image targets
			{
				m_render_target_manager = std::make_shared<RenderTargetManager>(m_context);
				ServiceLocator::Instance()->RegisterSystem<RenderTargetManager>(m_render_target_manager);
				VkExtent3D extent = m_window_manager.get()->VulkanGetWindowDimensions();
				m_render_target_manager->createDrawColorTarget(extent);
				m_render_target_manager->createDepthTarget(extent);
				
			}
			//8. Create Command Pools and Command Queues
			{
				m_command_pool_manager = std::make_shared<CommandPoolManager>(m_context);
				ServiceLocator::Instance()->RegisterSystem<CommandPoolManager>(m_command_pool_manager);
				m_command_pool_manager.get()->BuildCommandPools();
			}

			//9. Build Sync Structures
			{
				//I think these are only handled here?
				vk::FenceCreateInfo fence_info = {};
				fence_info.flags = vk::FenceCreateFlagBits::eSignaled;
				vk::SemaphoreCreateInfo semaphore_info = {};

				for (uint32_t i = 0 ; i < FRAMES; ++i)
				{
					frame_resources[i].fence = m_context.device.createFence(fence_info, nullptr);
					frame_resources[i].semaphore = m_context.device.createSemaphore(semaphore_info, nullptr);
				}
				
			}

			//10. Build Descriptor pools and sets
			{
				m_descriptor_manager = std::make_shared<DescriptorManager>(m_context);
				ServiceLocator::Instance()->RegisterSystem<DescriptorManager>(m_descriptor_manager);
				m_descriptor_manager.get()->createDescriptorPool();
				m_descriptor_manager.get()->createDescriptorSetLayout();
				m_descriptor_manager.get()->createDescriptorSet();
				
			}

			//11. handle shaders
			{
				Rendering::ShaderManager shaderManager;
				shaderManager.initialize();
				shaderManager.compileShader("shader","./assets/shaders/", vk::ShaderStageFlagBits::eFragment);
			}

			//11. Build the pipelines
			{
				/*
				// Create pipeline layout
				{
					auto layout = m_pipeline_manager.get()->createPipelineLayout(
						"main_layout",
						{m_descriptor_manager.get()->getDescriptorSet()},
						{pushConstantRange}
					);
				}
				

				
				{
					auto render_target_manager = m_render_target_manager.get();
					// Build pipeline
					auto pipeline = m_pipeline_manager.get()->getBuilder()
						.setPipelineType(PipelineManager::PipelineBuilder::PipelineType::Graphics)
						.addShaderStage(vk::ShaderStageFlagBits::eVertex, vertShader)
						.addShaderStage(vk::ShaderStageFlagBits::eFragment, fragShader)
						.addVertexBinding(0, sizeof(Vertex))
						.addVertexAttribute(0, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, pos))
						.setLayout(layout)
						.setColorFormats(render_target_manager->getColorRenderTarget().format)
						.setDepthFormat(render_target_manager->getDepthRenderTarget().format)
						.addDynamicState(vk::DynamicState::eViewport)
						.addDynamicState(vk::DynamicState::eScissor)
						.build();
				}

				// Cache it
				m_pipeline_manager.get()->cachePipeline("main_pipeline", pipeline, layout);
				*/
			}
#ifdef _DEBUG
			//12. initialize imgui
			{
				
			}
#endif
			//Done Initializing!
			return true;
		}

		void Render()
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(5000));
			//increment frame count, and reset back to 0
			current_frame = (current_frame + 1) % FRAMES;
		}

		void shutdown()
		{
			//waiting idle
			m_context.device.waitIdle();

			for (uint32_t i = 0 ; i < FRAMES; ++i)
			{
				m_context.device.destroySemaphore(frame_resources[i].semaphore);
				m_context.device.destroyFence(frame_resources[i].fence);
			}

			
			ServiceLocator::Instance()->Unregister<DescriptorManager>();
			m_descriptor_manager.reset();
			
			//destroy command pools
			ServiceLocator::Instance()->Unregister<CommandPoolManager>();
			m_command_pool_manager.reset();
			
			// Destroy rendertargets
			ServiceLocator::Instance()->Unregister<RenderTargetManager>();
			m_render_target_manager.reset();
			
			//execute deletion
			m_destructor.clear();

			//destroy swapchain and free swapchain manager
			ServiceLocator::Instance()->Unregister<SwapchainManager>();
			m_swapchain_manager.reset();
			
			m_context.instance.destroySurfaceKHR(m_context.surface);

			m_context.device.destroy();

			m_context.instance.destroyDebugUtilsMessengerEXT(m_context.debug_callback);

			m_context.instance.destroy();
			
			ServiceLocator::Instance()->Unregister<WindowManager>();
			m_window_manager.reset();
		}
	}; 
}