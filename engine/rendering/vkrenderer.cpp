module;

#include <vulkan/vulkan.h>

export module vkrenderer;

//TODO: find better names for the modules
import std;
import vkwindow;
import vkhelper;
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
			InitVulkan();
		}
		void Update()
		{
			
		}
		~vkrenderer()
		{
			
		}
	private:
		DeleteStack _destructor;
		VkInstance _instance = VK_NULL_HANDLE;
		VkDebugUtilsMessengerEXT _debug_messenger = VK_NULL_HANDLE;
		VkPhysicalDevice _selectedGraphicsCard = VK_NULL_HANDLE;
		VkDevice _logicalDevice = VK_NULL_HANDLE;

		//can request windows from it
		WindowManager _windowManager;

		//ugly functions, we will hide implementation details down the file
		void InitVulkan();
	};

	void vkrenderer::InitVulkan()
	{
		vkhelper::InstanceBuilder builder;
#ifdef _DEBUG
		builder.EnableDebugValidationLayers(true);
#endif
		builder.SetApplicationName("KitsuneEngine");
		builder.SetApplicationVersion(1, 0, 0);
		builder.SetEngineVersion(1, 0, 0);
		builder.SetVKAPIVersion(1, 4);
		
		try
		{
			_instance = builder.build();
		}
		catch (const std::exception& e)
		{
			std::cerr << e.what() << '\n';
		}
	}




}
