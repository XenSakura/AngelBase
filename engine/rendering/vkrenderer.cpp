module;


#include <vulkan/vulkan.hpp>
#define GLFW_INCLUDE_NONE
#include "GLFW/glfw3.h"

export module vkrenderer;

//TODO: find better names for the modules
import std;
import vkwindow;
import vkextensions;
import ServiceLocator;


//stuff within this rendering namespace should not be directly used except by the renderer! Do not touch 
namespace rendering
{
	//We're using this instead of RAII-- the GPU runs async from CPU
	//LIFO
	struct DeleteStack
	{
		std::stack<std::function<void()>> delete_functions;

		void push(std::function<void()> &&func)
		{
			delete_functions.push(func);
		}

		void clear()
		{
			while (!delete_functions.empty())
			{
				auto& function = delete_functions.top();
				function();
				delete_functions.pop();
			}
		}
	};
	
	export class vkrenderer : public ISystem
	{
	public:
		vkrenderer()
		{
			
		}
		void Update()
		{
			
		}
		~vkrenderer()
		{
			
		}

		void Init()
		{
			InitVulkan();
			//refactor into window manager eventually
			window = glfwCreateWindow(700, 900, "Kitsune Engine", nullptr, nullptr);
			InitDevices();
		}
	private:
		DeleteStack _destructor;

		struct vkcontext
		{
			vk::Instance _instance;
			vk::DebugUtilsMessengerEXT _debugCallback;
			vk::Device _device;
			vk::PhysicalDevice _physicalDevice;
			int32_t _graphics_queue_index = -1;
			vk::Queue _queue;
			vk::SurfaceKHR _surface;
		} context;

		GLFWwindow* window;

		

		void InitVulkan();
		void InitSurface();
		void InitDevices();
		void InitSwapchain();
	};

	
	
	
	
	
	void vkrenderer::InitVulkan()
	{
		//moving instance selection, debug callbacks and validation layers to vkextensions

		//Add the required instance extensions
		std::vector<const char *> required_instance_extensions{VK_KHR_SURFACE_EXTENSION_NAME};
		vkextensions::AddInstanceExtensions(required_instance_extensions);

		//init glfw and add glfw instance extensions
		glfwInit();
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		uint32_t glfw_extension_count = 0;
		const char ** glfw_instance_extensions = glfwGetRequiredInstanceExtensions(&glfw_extension_count);
		for (uint32_t i = 0; i < glfw_extension_count; i++)
		{
			required_instance_extensions.push_back(glfw_instance_extensions[i]);
		}
		
		//Add the required validation layers
		std::vector<const char *> requested_instance_layers{};
		vkextensions::AddInstanceLayers(requested_instance_layers);
		
		// TODO: allow these attributes to be edited elsewhere
		vk::ApplicationInfo app{
			.pApplicationName = "Test",
			.pEngineName = "KitsuneEngine",
			.apiVersion = VK_MAKE_VERSION(1, 4, 0)};

		vk::InstanceCreateInfo instance_info{.pApplicationInfo        = &app,
										 .enabledLayerCount       = static_cast<uint32_t>(requested_instance_layers.size()),
										 .ppEnabledLayerNames     = requested_instance_layers.data(),
										 .enabledExtensionCount   = static_cast<uint32_t>(required_instance_extensions.size()),
										 .ppEnabledExtensionNames = required_instance_extensions.data()};
		
		auto debug_info = vkextensions::CreateDebugMessengerInfo();
		if (debug_info)
		{
			instance_info.pNext = &(*debug_info);
		}
		
		context._instance = vk::createInstance(instance_info);

		if (debug_info)
		{
			context._debugCallback = context._instance.createDebugUtilsMessengerEXT(*debug_info);
		}

		//instance has been created
	}

	void vkrenderer::InitSurface()
	{
		if (context._surface)
		{
			context._instance.destroySurfaceKHR(context._surface);
		}
		
		VkSurfaceKHR surface = VK_NULL_HANDLE;
		VkResult e = glfwCreateWindowSurface(context._instance, window, nullptr, &surface);
		if (e != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create window surface");
		}
		context._surface = static_cast<vk::SurfaceKHR>(surface);
	}

	void PickPhysicalDevice()
	{
		
	}

	void vkrenderer::InitDevices()
	{
		// pick the GPU, Create the Surface, and grab the graphics queue index

		//split into smaller functions
		std::vector<vk::PhysicalDevice> physical_devices = context._instance.enumeratePhysicalDevices();

		for (const auto& physical_device : physical_devices)
		{
			vk::PhysicalDeviceProperties device_properties = physical_device.getProperties();
			if (device_properties.apiVersion >= VK_API_VERSION_1_2 < vk::ApiVersion14)
			{
				std::cerr << "Physical Device: " + std::string(device_properties.deviceName.data()) + " does not support Vulkan 1.4"<< std::endl;
			}

			
			std::vector<vk::QueueFamilyProperties> queue_family_properties = physical_device.getQueueFamilyProperties();

			// Find a queue family that works with the current surface
			auto qfpIt = std::ranges::find_if(queue_family_properties,
										  [&physical_device, surface = context._surface](vk::QueueFamilyProperties const &qfp) {
											  static uint32_t index = 0;
											  return (qfp.queueFlags & vk::QueueFlagBits::eGraphics) && physical_device.getSurfaceSupportKHR(index++, surface);
										  });

			if (qfpIt != queue_family_properties.end())
			{
				context._graphics_queue_index = std::distance(queue_family_properties.begin(), qfpIt);
				context._physicalDevice                  = physical_device;
				break;
			}
		}

		if (context._graphics_queue_index < 0)
		{
			throw std::runtime_error("Failed to find suitable GPU with Vulkan 1.4 support");
		}


		std::vector<vk::ExtensionProperties> device_extensions = context._physicalDevice.enumerateDeviceExtensionProperties();

		std::vector<const char*>required_device_extensions {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

		if (!vkextensions::validate_extensions(required_device_extensions,device_extensions))
		{
			throw std::runtime_error("Required Device Extensions are missing");
		}

		auto physical_device_features = context._physicalDevice.getFeatures2<vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan13Features, vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>()

		// Check if Physical device supports Vulkan 1.3 features
		if (!physical_device_features.get<vk::PhysicalDeviceVulkan13Features>().dynamicRendering)
		{
			throw std::runtime_error("Dynamic Rendering feature is missing");
		}
		if (!physical_device_features.get<vk::PhysicalDeviceVulkan13Features>().synchronization2)
		{
			throw std::runtime_error("Synchronization2 feature is missing");
		}
		if (!physical_device_features.get<vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>().extendedDynamicState)
		{
			throw std::runtime_error("Extended Dynamic State feature is missing");
		}

		// Enable only specific Vulkan 1.3 features
		vk::StructureChain<vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan13Features, vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>
			enabled_features_chain = {{}, {.synchronization2 = true, .dynamicRendering = true}, {.extendedDynamicState = true}};

		// Create the logical device
		float queue_priority = 0.5f;

		// Create one queue
		vk::DeviceQueueCreateInfo queue_info{.queueFamilyIndex = static_cast<uint32_t>(context._graphics_queue_index),
											 .queueCount       = 1,
											 .pQueuePriorities = &queue_priority};

		vk::DeviceCreateInfo device_info{.pNext                   = &enabled_features_chain.get<vk::PhysicalDeviceFeatures2>(),
										 .queueCreateInfoCount    = 1,
										 .pQueueCreateInfos       = &queue_info,
										 .enabledExtensionCount   = static_cast<uint32_t>(required_device_extensions.size()),
										 .ppEnabledExtensionNames = required_device_extensions.data()};

		context._device = context._physicalDevice.createDevice(device_info);

		context._queue = context._device.getQueue(context._graphics_queue_index, 0);
	}

	void vkrenderer::InitSwapchain()
	{
		
	}


	



}
